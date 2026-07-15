// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "AI/WTBRBasicBotController.h"
#include "WTBRCharacter.h"
#include "Components/WTBRHealthComponent.h"
#include "Trigger/WTBRAegornTrigger.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "EngineUtils.h"

AWTBRBasicBotController::AWTBRBasicBotController()
{
    PrimaryActorTick.bCanEverTick = true;
    bAllowStrafe = true;
}

void AWTBRBasicBotController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    CombatTarget.Reset();
    NextDecisionTime = 0.0f;
    NextFireTime = 0.0f;
    NextDefenseTime = 0.0f;
}

void AWTBRBasicBotController::OnUnPossess()
{
    StopMovement();
    ClearFocus(EAIFocusPriority::Gameplay);
    CombatTarget.Reset();
    Super::OnUnPossess();
}

void AWTBRBasicBotController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    AWTBRCharacter* BotCharacter = Cast<AWTBRCharacter>(GetPawn());
    UWorld* World = GetWorld();
    if (!IsValid(BotCharacter) || !World || !BotCharacter->HasAuthority()
        || !IsValid(BotCharacter->HealthComponent) || !BotCharacter->HealthComponent->IsAlive())
    {
        StopMovement();
        return;
    }

    const float Now = World->GetTimeSeconds();
    if (Now < NextDecisionTime) return;
    NextDecisionTime = Now + FMath::Max(DecisionInterval, 0.05f);

    AcquireClosestRadarContact(BotCharacter);
    UpdateCombat(BotCharacter, Now);
}

void AWTBRBasicBotController::AcquireClosestRadarContact(AWTBRCharacter* BotCharacter)
{
    UWorld* World = GetWorld();
    if (!World) return;

    AWTBRCharacter* BestTarget = nullptr;
    float BestDistanceSquared = FMath::Square(RadarSearchRange);
    for (TActorIterator<AWTBRCharacter> It(World); It; ++It)
    {
        AWTBRCharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate == BotCharacter || Candidate->IsRadarCloaked()
            || BotCharacter->IsSameTeamAs(Candidate) || !IsValid(Candidate->HealthComponent)
            || !Candidate->HealthComponent->IsAlive())
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(
            Candidate->GetActorLocation(), BotCharacter->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = Candidate;
        }
    }
    CombatTarget = BestTarget;
}

void AWTBRBasicBotController::UpdateCombat(AWTBRCharacter* BotCharacter, float Now)
{
    AWTBRCharacter* Target = CombatTarget.Get();
    if (!IsValid(Target))
    {
        StopMovement();
        ClearFocus(EAIFocusPriority::Gameplay);
        return;
    }
    if (!IsValid(BotCharacter->TriggerSetComponent)) return;

    // Bot v1 has a deliberately locked loadout contract: Main is the attack
    // trigger and active Sub is Aegorn. A bot without that defensive slot does
    // not enter combat rather than silently becoming an unbalanced attacker.
    UWTBRAegornTrigger* Aegorn = nullptr;
    if (!HasLockedAegornLoadout(BotCharacter, Aegorn))
    {
        StopMovement();
        return;
    }

    UWTBRTriggerDataAsset* MainDataAsset = BotCharacter->TriggerSetComponent->GetActiveMainDataAsset();
    if (!IsValid(MainDataAsset) || !IsCombatCategory(MainDataAsset->Category))
    {
        StopMovement();
        return;
    }

    const float PreferredRange = GetPreferredCombatRange(MainDataAsset);
    const float Distance = FVector::Dist2D(BotCharacter->GetActorLocation(), Target->GetActorLocation());
    SetFocus(Target, EAIFocusPriority::Gameplay);
    const FRotator AimRotation = (Target->GetActorLocation() - BotCharacter->GetActorLocation()).Rotation();
    SetControlRotation(AimRotation);

    TryRaiseAegorn(BotCharacter, Aegorn, Distance, Now);

    if (Distance > PreferredRange)
    {
        MoveToActor(Target, FMath::Max(PreferredRange * 0.8f, 100.0f), true, true, false, nullptr, true);
        return;
    }

    StopMovement();
    if (Now < NextFireTime || !LineOfSightTo(Target) || !BotCharacter->CanAffordActiveMainTriggerForHUD()) return;

    // A zero-duration press/release is intentionally a tap. Hold/charge behavior
    // is left to the next bot tier, where each trigger can advertise its profile.
    BotCharacter->ExecuteBotTriggerInput(true, true);
    BotCharacter->ExecuteBotTriggerInput(true, false);
    NextFireTime = Now + FMath::Max(FireInterval, 0.1f);
}

bool AWTBRBasicBotController::HasLockedAegornLoadout(
    const AWTBRCharacter* BotCharacter, UWTBRAegornTrigger*& OutAegorn) const
{
    OutAegorn = nullptr;
    if (!IsValid(BotCharacter) || !IsValid(BotCharacter->TriggerSetComponent)) return false;

    OutAegorn = Cast<UWTBRAegornTrigger>(BotCharacter->TriggerSetComponent->GetActiveSubTrigger());
    return IsValid(OutAegorn);
}

void AWTBRBasicBotController::TryRaiseAegorn(
    AWTBRCharacter* BotCharacter, UWTBRAegornTrigger* Aegorn, float TargetDistance, float Now)
{
    if (!IsValid(BotCharacter) || !IsValid(Aegorn) || Aegorn->IsShieldActive() || Now < NextDefenseTime) return;

    const UWTBRHealthComponent* Health = BotCharacter->HealthComponent;
    const float HealthFraction = IsValid(Health) && Health->GetMaxHP() > KINDA_SMALL_NUMBER
        ? Health->GetCurrentHP() / Health->GetMaxHP() : 1.0f;
    if (TargetDistance > AegornDefenseRange && HealthFraction > AegornLowHealthFraction) return;

    if (!BotCharacter->CanAffordActiveSubTriggerForHUD()) return;

    // A short Sub press/release selects Aegorn's planted, forward-facing shield.
    // Aegorn's tap path reads actor rotation, so align only at the instant of
    // spawning instead of fighting CharacterMovement while the bot is moving.
    BotCharacter->SetActorRotation(FRotator(0.0f, GetControlRotation().Yaw, 0.0f));
    BotCharacter->ExecuteBotTriggerInput(false, true);
    BotCharacter->ExecuteBotTriggerInput(false, false);
    NextDefenseTime = Aegorn->IsShieldActive()
        ? Now + FMath::Max(AegornDefenseInterval, 0.1f)
        : Now + 0.25f;
}

float AWTBRBasicBotController::GetPreferredCombatRange(const UWTBRTriggerDataAsset* DataAsset) const
{
    if (!IsValid(DataAsset)) return 0.0f;

    switch (DataAsset->Category)
    {
        case ETriggerCategory::Melee:
            return FMath::Max(DataAsset->MeleeHitbox.ForwardOffset * 1.25f, 250.0f);
        case ETriggerCategory::Gunner:
            return 1400.0f;
        case ETriggerCategory::SniperBullet:
            return 3200.0f;
        default:
            return 0.0f;
    }
}

bool AWTBRBasicBotController::IsCombatCategory(ETriggerCategory Category)
{
    return Category == ETriggerCategory::Melee
        || Category == ETriggerCategory::Gunner
        || Category == ETriggerCategory::SniperBullet;
}
