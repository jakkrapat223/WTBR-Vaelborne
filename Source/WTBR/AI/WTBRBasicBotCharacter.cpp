// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "AI/WTBRBasicBotCharacter.h"
#include "AI/WTBRBasicBotController.h"
#include "AI/WTBRBotLoadoutSetDataAsset.h"
#include "Trigger/WTBRAegornTrigger.h"
#include "WTBRGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWTBRBasicBotCharacter::AWTBRBasicBotCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    AIControllerClass = AWTBRBasicBotController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AWTBRBasicBotCharacter::BeginPlay()
{
    Super::BeginPlay();
    TryAssignApprovedLoadout();
}

void AWTBRBasicBotCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LoadoutAssignmentRetryTimer);
    }
    Super::EndPlay(EndPlayReason);
}

void AWTBRBasicBotCharacter::TryAssignApprovedLoadout()
{
    if (bApprovedLoadoutAssigned || !HasAuthority() || !GetWorld()) return;

    if (ApprovedLoadoutSet.IsNull())
    {
        if (!bMissingLoadoutSetLogged)
        {
            bMissingLoadoutSetLogged = true;
            UE_LOG(LogTemp, Warning, TEXT("WTBR Bot %s has no ApprovedLoadoutSet; it will retain its authored loadout."),
                *GetNameSafe(this));
        }
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if (LoadoutAssignmentStartTime < 0.0f)
    {
        LoadoutAssignmentStartTime = Now;
    }

    AWTBRGameState* GameState = GetWorld()->GetGameState<AWTBRGameState>();
    if (!IsValid(GameState) || !GameState->IsLoadoutSetupAllowedPhase())
    {
        ++LoadoutAssignmentRetryCount;
        const bool bTimedOut = Now - LoadoutAssignmentStartTime >= LoadoutAssignmentTimeoutSeconds
            || LoadoutAssignmentRetryCount >= MaxLoadoutAssignmentRetries;
        if (bTimedOut)
        {
            if (!bLoadoutAssignmentTimeoutLogged)
            {
                bLoadoutAssignmentTimeoutLogged = true;
                UE_LOG(LogTemp, Warning,
                    TEXT("WTBR Bot %s could not assign ApprovedLoadoutSet before LoadoutSetup closed (retries=%d, elapsed=%.2fs)."),
                    *GetNameSafe(this), LoadoutAssignmentRetryCount, Now - LoadoutAssignmentStartTime);
            }
            return;
        }

        if (LoadoutAssignmentRetryCount == 1)
        {
            UE_LOG(LogTemp, Verbose, TEXT("WTBR Bot %s waiting for LoadoutSetup before assigning its approved loadout."),
                *GetNameSafe(this));
        }
        GetWorld()->GetTimerManager().SetTimer(LoadoutAssignmentRetryTimer, this,
            &AWTBRBasicBotCharacter::TryAssignApprovedLoadout, 0.25f, false);
        return;
    }

    UWTBRBotLoadoutSetDataAsset* LoadoutSet = ApprovedLoadoutSet.LoadSynchronous();
    if (!IsValid(LoadoutSet) || !IsValid(TriggerSetComponent)) return;

    TMap<EWTBRBotCombatRole, TArray<const FWTBRApprovedBotLoadout*>> ValidByRole;
    for (const FWTBRApprovedBotLoadout& Entry : LoadoutSet->ApprovedLoadouts)
    {
        UWTBRTriggerDataAsset* Main = Entry.MainTrigger.LoadSynchronous();
        UWTBRTriggerDataAsset* Aegorn = Entry.AegornSubTrigger.LoadSynchronous();
        const ETriggerCategory ExpectedCategory = Entry.Role == EWTBRBotCombatRole::Gunner
            ? ETriggerCategory::Gunner
            : Entry.Role == EWTBRBotCombatRole::Sniper
                ? ETriggerCategory::SniperBullet
                : ETriggerCategory::Melee;

        const bool bMainValid = IsValid(Main) && Main->Category == ExpectedCategory
            && Main->SlotConstraint != ETriggerSlotConstraint::SubOnly && Main->TriggerClass != nullptr;
        const bool bAegornValid = IsValid(Aegorn) && Aegorn->Category == ETriggerCategory::Defense
            && Aegorn->SlotConstraint != ETriggerSlotConstraint::MainOnly
            && Aegorn->TriggerClass && Aegorn->TriggerClass->IsChildOf(UWTBRAegornTrigger::StaticClass());
        if (bMainValid && bAegornValid)
        {
            ValidByRole.FindOrAdd(Entry.Role).Add(&Entry);
        }
    }

    TArray<EWTBRBotCombatRole> AvailableRoles;
    ValidByRole.GetKeys(AvailableRoles);
    if (AvailableRoles.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR Bot %s has no valid approved Main+Aegorn loadout entries."), *GetNameSafe(this));
        return;
    }

    const EWTBRBotCombatRole SelectedRole = AvailableRoles[FMath::RandHelper(AvailableRoles.Num())];
    const TArray<const FWTBRApprovedBotLoadout*>& Candidates = ValidByRole.FindChecked(SelectedRole);
    const FWTBRApprovedBotLoadout& Selected = *Candidates[FMath::RandHelper(Candidates.Num())];

    TArray<TSoftObjectPtr<UWTBRTriggerDataAsset>> LockedLoadout;
    LockedLoadout.Init(nullptr, UWTBRTriggerSetComponent::TotalSlotCount);
    LockedLoadout[0] = Selected.MainTrigger;
    LockedLoadout[UWTBRTriggerSetComponent::MainSlotCount] = Selected.AegornSubTrigger;
    TriggerSetComponent->Server_SetTriggerLoadout(LockedLoadout);
    TriggerSetComponent->SwitchMainSlot(0);
    TriggerSetComponent->SwitchSubSlot(0);
    bApprovedLoadoutAssigned = true;

    UE_LOG(LogTemp, Log, TEXT("WTBR Bot %s assigned approved role %s (Main=%s, Sub=Aegorn=%s)."),
        *GetNameSafe(this), *UEnum::GetValueAsString(SelectedRole),
        *Selected.MainTrigger.ToSoftObjectPath().ToString(), *Selected.AegornSubTrigger.ToSoftObjectPath().ToString());
}
