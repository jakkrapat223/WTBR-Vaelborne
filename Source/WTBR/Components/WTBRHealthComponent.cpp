// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Components/WTBRHealthComponent.h"
#include "WTBRValidationLog.h"
#include "Data/WTBRCoreStatsDataAsset.h"
#include "Net/UnrealNetwork.h"
#include "WTBRCharacter.h"
#include "WTBRGameState.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/WTBRCorpseLootContainerActor.h"
#include "Interaction/WTBRDroppedTriggerActor.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

// Drain fires every 0.5 s on Authority; HP removed = drainRate * MaxHP * interval per limb
static const float LIMB_DRAIN_TICK_INTERVAL = 0.5f;

namespace
{
static TAutoConsoleVariable<int32> CVarWTBRUseCorpseLootContainerOnDeath(
    TEXT("WTBR.UseCorpseLootContainerOnDeath"),
    0,
    TEXT("-1 = use MatchModeRules, 0 = legacy dropped-trigger death loot path, 1 = corpse loot container death loot path."),
    ECVF_Default);

void LogTest32HealthCoreStats(const UWTBRHealthComponent* Component, const UWTBRCoreStatsDataAsset* Stats, const TCHAR* Requester)
{
    if (Stats)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Loaded] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsAsset=%s | CoreStatsPath=%s | StatsValid=true | MaxHP=%.1f | MaxDownedHP=%.1f | KnockdownIFrameDuration=%.2f"),
            *GetNameSafe(Component),
            *GetNameSafe(Component ? Component->GetOwner() : nullptr),
            Component ? (int32)Component->GetNetMode() : -1,
            Component ? (int32)Component->GetOwnerRole() : -1,
            Requester ? Requester : TEXT("Unknown"),
            *GetNameSafe(Stats),
            Component ? *Component->CoreStatsAsset.ToSoftObjectPath().ToString() : TEXT("None"),
            Stats->MaxHP,
            Stats->MaxDownedHP,
            Stats->KnockdownIFrameDuration);
        return;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test32 CoreStats Missing] Component=%s | Owner=%s | NetMode=%d | Role=%d | Requester=%s | CoreStatsPath=%s | StatsValid=false"),
        *GetNameSafe(Component),
        *GetNameSafe(Component ? Component->GetOwner() : nullptr),
        Component ? (int32)Component->GetNetMode() : -1,
        Component ? (int32)Component->GetOwnerRole() : -1,
        Requester ? Requester : TEXT("Unknown"),
        Component ? *Component->CoreStatsAsset.ToSoftObjectPath().ToString() : TEXT("None"));
}
}

UWTBRHealthComponent::UWTBRHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    // 5 slots: index = EWTBRLimbType value (0=None, 1=LeftArm … 4=RightLeg)
    LimbStates.Init(FWTBRLimbState(), 5);
}

void UWTBRHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    const UWTBRCoreStatsDataAsset* LoadedStats = GetStats();
    LogTest32HealthCoreStats(this, LoadedStats, TEXT("BeginPlay"));
    UE_LOG(LogTemp, Warning,
        TEXT("[Health BeginPlay] Owner=%s | Component=%s | CoreStatsAsset=%s | CoreStatsPath=%s | HasAuthority=%s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(this),
        *GetNameSafe(LoadedStats),
        *CoreStatsAsset.ToSoftObjectPath().ToString(),
        GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));

    CurrentHP = LoadedStats ? LoadedStats->MaxHP : GetMaxHP();
    CurrentDownedHP = 0.0f;
    CurrentCombatState = EWTBRCombatState::Alive;
    bIsInvulnerable = false;
    if (LimbStates.Num() != 5) LimbStates.Init(FWTBRLimbState(), 5);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void UWTBRHealthComponent::ApplyDamage(float DamageAmount, AActor* DamageInstigator)
{
    const float OldHP = CurrentHP;
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Start | Target=%s | Auth=%s | Instigator=%s | OldHP=%.1f | Damage=%.1f | State=%d | Invulnerable=%s | ApplyDamageCalled=true"),
        *GetNameSafe(GetOwner()),
        GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"),
        *GetNameSafe(DamageInstigator),
        OldHP,
        DamageAmount,
        static_cast<int32>(CurrentCombatState),
        bIsInvulnerable ? TEXT("true") : TEXT("false"));

    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Rejected | Target=%s | Reason=NoAuthorityOrOwnerInvalid | Damage=%.1f"),
            *GetNameSafe(GetOwner()),
            DamageAmount);
        return;
    }
    if (DamageAmount <= 0.0f)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Rejected | Target=%s | Reason=NonPositiveDamage | Damage=%.1f"),
            *GetNameSafe(GetOwner()),
            DamageAmount);
        return;
    }
    if (CurrentCombatState == EWTBRCombatState::Eliminated)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Rejected | Target=%s | Reason=Eliminated | Damage=%.1f"),
            *GetNameSafe(GetOwner()),
            DamageAmount);
        return;
    }
    if (CurrentCombatState == EWTBRCombatState::Downed && bIsInvulnerable)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Test28 Server DamageIgnored_Invulnerable] Owner=%s | NetMode=%d | Role=%d | State=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s | DamageAmount=%.1f"),
            *GetNameSafe(GetOwner()),
            (int32)GetNetMode(),
            GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
            (int32)CurrentCombatState,
            CurrentHP,
            CurrentDownedHP,
            bIsInvulnerable ? TEXT("true") : TEXT("false"),
            DamageAmount);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Rejected | Target=%s | Reason=DownedInvulnerable | OldHP=%.1f | Damage=%.1f"),
            *GetNameSafe(GetOwner()),
            OldHP,
            DamageAmount);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR ApplyDamage: %.0f → HP %.0f→%.0f on %s"),
        DamageAmount, CurrentHP, FMath::Max(0.f, CurrentHP - DamageAmount),
        *GetOwner()->GetName());
    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Red,
            FString::Printf(TEXT("[HP] %.0f → %.0f  (-%0.f)"),
                CurrentHP, FMath::Max(0.f, CurrentHP - DamageAmount), DamageAmount));
    }

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        AWTBRCharacter* OwnerChar = Cast<AWTBRCharacter>(GetOwner());
        if (OwnerChar && OwnerChar->TriggerSetComponent &&
            OwnerChar->TriggerSetComponent->GetCurrentMergeState() != EWTBRCompositeBulletType::None)
        {
            OwnerChar->TriggerSetComponent->CancelMerge();
        }
    }

    RecordDamageContribution(DamageAmount, DamageInstigator);

    if (CurrentCombatState == EWTBRCombatState::Alive)
    {
        CurrentHP = FMath::Max(0.f, CurrentHP - DamageAmount);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Result | Target=%s | Instigator=%s | OldHP=%.1f | Damage=%.1f | NewHP=%.1f | State=Alive"),
            *GetNameSafe(GetOwner()),
            *GetNameSafe(DamageInstigator),
            OldHP,
            DamageAmount,
            CurrentHP);
        OnHPChanged.Broadcast(CurrentHP, GetMaxHP());
        if (CurrentHP <= 0.f)
        {
            EnterDownedState(DamageInstigator);
        }
        return;
    }

    if (CurrentCombatState == EWTBRCombatState::Downed)
    {
        const float OldDownedHP = CurrentDownedHP;
        CurrentDownedHP = FMath::Max(0.f, CurrentDownedHP - DamageAmount);
        WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] ApplyDamage Result | Target=%s | Instigator=%s | OldHP=%.1f | OldDownedHP=%.1f | Damage=%.1f | NewDownedHP=%.1f | State=Downed"),
            *GetNameSafe(GetOwner()),
            *GetNameSafe(DamageInstigator),
            OldHP,
            OldDownedHP,
            DamageAmount,
            CurrentDownedHP);
        if (CurrentDownedHP <= 0.f)
        {
            EnterEliminatedState(DamageInstigator);
        }
    }
}

void UWTBRHealthComponent::ResetCombatRewardState()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(KnockdownIFrameTimerHandle);
    }

    ClearDamageHistory(TEXT("ResetCombatRewardState"));
    bDownRewardResolved = false;
    bDeathRewardsResolved = false;
    CurrentDownedHP = 0.0f;
    CurrentHP = GetMaxHP();
    OnHPChanged.Broadcast(CurrentHP, GetMaxHP());
    SetInvulnerable(false);
    SetCombatState(EWTBRCombatState::Alive);
}

void UWTBRHealthComponent::ClearDamageHistory(const TCHAR* Source)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Reward DamageHistory Clear] Source=%s | Victim=%s | CountBefore=%d"),
        Source ? Source : TEXT("Unknown"),
        *GetNameSafe(GetOwner()),
        DamageHistory.Num());

    DamageHistory.Reset();
}

void UWTBRHealthComponent::SetCombatState(EWTBRCombatState NewState)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (CurrentCombatState == NewState) return;

    const EWTBRCombatState OldState = CurrentCombatState;
    CurrentCombatState = NewState;
    OnCombatStateChanged.Broadcast(CurrentCombatState, OldState);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test27 Server SetCombatState] Owner=%s | NetMode=%d | Role=%d | OldState=%d | NewState=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
        (int32)OldState,
        (int32)CurrentCombatState,
        CurrentHP,
        CurrentDownedHP,
        bIsInvulnerable ? TEXT("true") : TEXT("false"));

    if (CurrentCombatState == EWTBRCombatState::Downed)
    {
        OnDowned.Broadcast();
    }
    else if (CurrentCombatState == EWTBRCombatState::Eliminated)
    {
        OnDeath.Broadcast();
    }
}

void UWTBRHealthComponent::SetInvulnerable(bool bNewInvulnerable)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsInvulnerable == bNewInvulnerable) return;

    bIsInvulnerable = bNewInvulnerable;
    OnInvulnerabilityChanged.Broadcast(bIsInvulnerable);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test28 Server SetInvulnerable] Owner=%s | NetMode=%d | Role=%d | State=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
        (int32)CurrentCombatState,
        CurrentHP,
        CurrentDownedHP,
        bIsInvulnerable ? TEXT("true") : TEXT("false"));
}

void UWTBRHealthComponent::EnterDownedState(AActor* DownInstigator)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (CurrentCombatState != EWTBRCombatState::Alive) return;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test27 Server EnterDownedState] Owner=%s | NetMode=%d | Role=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
        CurrentHP,
        CurrentDownedHP,
        bIsInvulnerable ? TEXT("true") : TEXT("false"));

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("EnterDownedState"));
    UE_LOG(LogTemp, Warning,
        TEXT("[EnterDownedState CoreStatsCheck] Owner=%s | Component=%s | CoreStatsAsset=%s | CoreStatsPath=%s | HP=%.1f | DownedHP=%.1f | State=%d | HasAuthority=%s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(this),
        *GetNameSafe(S),
        *CoreStatsAsset.ToSoftObjectPath().ToString(),
        CurrentHP,
        CurrentDownedHP,
        static_cast<uint8>(CurrentCombatState),
        GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));

    if (!S)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR DownedState: CoreStats missing | Owner=%s | OwnerClass=%s | Component=%s | CoreStatsAsset=%s | CoreStatsPath=%s | HasAuthority=%s"),
            *GetNameSafe(GetOwner()),
            GetOwner() ? *GetNameSafe(GetOwner()->GetClass()) : TEXT("None"),
            *GetNameSafe(this),
            *GetNameSafe(S),
            *CoreStatsAsset.ToSoftObjectPath().ToString(),
            GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"));
        EnterEliminatedState(DownInstigator);
        return;
    }

    CurrentHP = 0.0f;
    OnHPChanged.Broadcast(CurrentHP, GetMaxHP());
    CurrentDownedHP = S->MaxDownedHP;

    ResolveDownReward(DownInstigator);
    StartKnockdownIFrame();
    SetCombatState(EWTBRCombatState::Downed);
}

void UWTBRHealthComponent::EnterEliminatedState(AActor* FinalDamageInstigator)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (CurrentCombatState == EWTBRCombatState::Eliminated) return;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test27 Server EnterEliminatedState] Owner=%s | NetMode=%d | Role=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
        CurrentHP,
        CurrentDownedHP,
        bIsInvulnerable ? TEXT("true") : TEXT("false"));

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(KnockdownIFrameTimerHandle);
    }

    CurrentDownedHP = 0.0f;
    SetInvulnerable(false);
    ResolveDeathRewards(FinalDamageInstigator);
    SpawnDroppedTriggersForEliminatedCharacter();
    SetCombatState(EWTBRCombatState::Eliminated);
}

void UWTBRHealthComponent::ResolveDownReward(AActor* DownInstigator)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bDownRewardResolved) return;

    bDownRewardResolved = true;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("ResolveDownReward"));
    AWTBRCharacter* VictimCharacter = Cast<AWTBRCharacter>(GetOwner());
    AWTBRCharacter* DownCharacter = ResolveContributorCharacter(DownInstigator);
    if (!S || !IsValid(VictimCharacter) || !IsValid(DownCharacter) || DownCharacter == VictimCharacter)
    {
        UE_LOG(LogTemp, Log,
            TEXT("WTBR DownReward: skipped environmental/self down reward for %s"),
            *GetOwner()->GetName());
        return;
    }

    GrantVaelReward(DownCharacter, S->VaelRewardOnDown);
}

void UWTBRHealthComponent::StartKnockdownIFrame()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("StartKnockdownIFrame"));
    if (!S || S->KnockdownIFrameDuration <= 0.0f)
    {
        SetInvulnerable(false);
        return;
    }

    SetInvulnerable(true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            KnockdownIFrameTimerHandle,
            this, &UWTBRHealthComponent::EndKnockdownIFrame,
            S->KnockdownIFrameDuration,
            false);
    }
}

void UWTBRHealthComponent::EndKnockdownIFrame()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SetInvulnerable(false);
}

AWTBRCharacter* UWTBRHealthComponent::ResolveContributorCharacter(AActor* DamageInstigator) const
{
    auto TryResolveCharacter = [](AActor* Candidate) -> AWTBRCharacter*
    {
        if (!IsValid(Candidate)) return nullptr;

        if (AWTBRCharacter* Character = Cast<AWTBRCharacter>(Candidate))
        {
            return Character;
        }

        if (AController* Controller = Cast<AController>(Candidate))
        {
            return Cast<AWTBRCharacter>(Controller->GetPawn());
        }

        if (APawn* Pawn = Cast<APawn>(Candidate))
        {
            return Cast<AWTBRCharacter>(Pawn);
        }

        return nullptr;
    };

    TArray<AActor*> Candidates;
    if (IsValid(DamageInstigator))
    {
        Candidates.Add(DamageInstigator);

        if (APawn* InstigatorPawn = DamageInstigator->GetInstigator())
        {
            Candidates.Add(InstigatorPawn);
        }

        if (AActor* OwnerActor = DamageInstigator->GetOwner())
        {
            Candidates.Add(OwnerActor);

            if (APawn* OwnerInstigator = OwnerActor->GetInstigator())
            {
                Candidates.Add(OwnerInstigator);
            }
        }
    }

    for (AActor* Candidate : Candidates)
    {
        if (AWTBRCharacter* Character = TryResolveCharacter(Candidate))
        {
            return Character;
        }
    }

    return nullptr;
}

AController* UWTBRHealthComponent::ResolveContributorController(AWTBRCharacter* ContributorCharacter) const
{
    return IsValid(ContributorCharacter) ? ContributorCharacter->GetController() : nullptr;
}

void UWTBRHealthComponent::RecordDamageContribution(float DamageAmount, AActor* DamageInstigator)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (DamageAmount <= 0.0f) return;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("RecordDamageContribution"));
    if (!S) return;

    AWTBRCharacter* VictimCharacter = Cast<AWTBRCharacter>(GetOwner());
    AWTBRCharacter* ContributorCharacter = ResolveContributorCharacter(DamageInstigator);
    const int32 CountBefore = DamageHistory.Num();
    const EWTBRCombatState StateBefore = CurrentCombatState;
    if (!IsValid(ContributorCharacter) || ContributorCharacter == VictimCharacter)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Reward RecordContribution] Victim=%s | Attacker=%s | Damage=%.1f | StateBefore=%d | DamageHistoryCountBefore=%d | DamageHistoryCountAfter=%d"),
            *GetNameSafe(VictimCharacter),
            *GetNameSafe(ContributorCharacter),
            DamageAmount,
            static_cast<uint8>(StateBefore),
            CountBefore,
            DamageHistory.Num());
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    const float CurrentTime = World->GetTimeSeconds();

    for (FWTBRDamageContributionRecord& Record : DamageHistory)
    {
        if (Record.ContributorCharacter.Get() == ContributorCharacter)
        {
            Record.TotalDamage += DamageAmount;
            Record.LastDamageTime = CurrentTime;
            Record.ContributorController = ResolveContributorController(ContributorCharacter);
            WTBR_VALIDATION_LOG(Verbose, TEXT("[Reward RecordContribution] Victim=%s | Attacker=%s | Damage=%.1f | StateBefore=%d | DamageHistoryCountBefore=%d | DamageHistoryCountAfter=%d"),
                *GetNameSafe(VictimCharacter),
                *GetNameSafe(ContributorCharacter),
                DamageAmount,
                static_cast<uint8>(StateBefore),
                CountBefore,
                DamageHistory.Num());
            return;
        }
    }

    FWTBRDamageContributionRecord NewRecord;
    NewRecord.ContributorCharacter = ContributorCharacter;
    NewRecord.ContributorController = ResolveContributorController(ContributorCharacter);
    NewRecord.TotalDamage = DamageAmount;
    NewRecord.LastDamageTime = CurrentTime;
    DamageHistory.Add(NewRecord);

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Reward RecordContribution] Victim=%s | Attacker=%s | Damage=%.1f | StateBefore=%d | DamageHistoryCountBefore=%d | DamageHistoryCountAfter=%d"),
        *GetNameSafe(VictimCharacter),
        *GetNameSafe(ContributorCharacter),
        DamageAmount,
        static_cast<uint8>(StateBefore),
        CountBefore,
        DamageHistory.Num());
}

void UWTBRHealthComponent::ResolveDeathRewards(AActor* FinalDamageInstigator)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    AWTBRCharacter* VictimCharacter = Cast<AWTBRCharacter>(GetOwner());
    AWTBRCharacter* KillerCharacter = ResolveContributorCharacter(FinalDamageInstigator);

    UE_LOG(LogTemp, Warning,
        TEXT("[Reward Resolve Start] Victim=%s | Killer=%s | bDeathRewardsResolved=%s | DamageHistoryCount=%d"),
        *GetNameSafe(VictimCharacter),
        *GetNameSafe(KillerCharacter),
        bDeathRewardsResolved ? TEXT("true") : TEXT("false"),
        DamageHistory.Num());

    if (bDeathRewardsResolved)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Reward Resolve End]"));
        return;
    }

    bDeathRewardsResolved = true;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("ResolveDeathRewards"));
    UWorld* World = GetWorld();
    if (!S || !IsValid(VictimCharacter) || !World)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("WTBR DeathReward: skipped reward resolution for %s because CoreStats, victim, or world is invalid"),
            *GetOwner()->GetName());
        ClearDamageHistory(TEXT("ResolveDeathRewards_InvalidState"));
        UE_LOG(LogTemp, Warning, TEXT("[Reward Resolve End]"));
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();

    if (!IsValid(KillerCharacter) || KillerCharacter == VictimCharacter)
    {
        UE_LOG(LogTemp, Log,
            TEXT("WTBR DeathReward: skipped environmental/self elimination reward for %s"),
            *GetOwner()->GetName());
        ClearDamageHistory(TEXT("ResolveDeathRewards_NoValidKiller"));
        UE_LOG(LogTemp, Warning, TEXT("[Reward Resolve End]"));
        return;
    }

    TArray<AWTBRCharacter*> AssistRecipients;

    for (const FWTBRDamageContributionRecord& Record : DamageHistory)
    {
        AWTBRCharacter* ContributorCharacter = Record.ContributorCharacter.Get();
        const float Age = CurrentTime - Record.LastDamageTime;
        bool bQualifiesAssist = false;
        const TCHAR* Reason = TEXT("Qualifies");

        if (!IsValid(ContributorCharacter))
        {
            Reason = TEXT("InvalidContributor");
        }
        else if (ContributorCharacter == VictimCharacter)
        {
            Reason = TEXT("Victim");
        }
        else if (ContributorCharacter == KillerCharacter)
        {
            Reason = TEXT("Killer");
        }
        else if (Record.TotalDamage < S->AssistMinimumDamage)
        {
            Reason = TEXT("BelowMinimumDamage");
        }
        else if (Age > S->AssistTimeWindow)
        {
            Reason = TEXT("Expired");
        }
        else
        {
            bQualifiesAssist = true;
            AssistRecipients.Add(ContributorCharacter);
        }

        UE_LOG(LogTemp, Warning,
            TEXT("[Reward Contributor] Contributor=%s | Damage=%.1f | Age=%.2f | QualifiesAssist=%s | Reason=%s"),
            *GetNameSafe(ContributorCharacter),
            Record.TotalDamage,
            Age,
            bQualifiesAssist ? TEXT("true") : TEXT("false"),
            Reason);
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Reward Grant Kill] Receiver=%s | Amount=%.1f"),
        *GetNameSafe(KillerCharacter),
        S->VaelRewardOnKill);
    GrantVaelReward(KillerCharacter, S->VaelRewardOnKill);

    for (AWTBRCharacter* AssistRecipient : AssistRecipients)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Reward Grant Assist] Receiver=%s | Amount=%.1f"),
            *GetNameSafe(AssistRecipient),
            S->VaelRewardOnAssist);
        GrantVaelReward(AssistRecipient, S->VaelRewardOnAssist);
    }

    ClearDamageHistory(TEXT("ResolveDeathRewards_Complete"));
    UE_LOG(LogTemp, Warning, TEXT("[Reward Resolve End]"));
}

void UWTBRHealthComponent::GrantVaelReward(AWTBRCharacter* RecipientCharacter, float Amount) const
{
    if (!IsValid(RecipientCharacter) || !RecipientCharacter->HasAuthority()) return;
    if (Amount <= 0.0f) return;

    UWTBRVaelComponent* VaelComponent = RecipientCharacter->FindComponentByClass<UWTBRVaelComponent>();
    if (!IsValid(VaelComponent)) return;

    VaelComponent->GrantVael(Amount);
}

void UWTBRHealthComponent::SpawnDroppedTriggersForEliminatedCharacter()
{
    const int32 CorpseLootContainerMode = CVarWTBRUseCorpseLootContainerOnDeath.GetValueOnGameThread();
    bool bUseCorpseLootContainer = CorpseLootContainerMode == 1;

    if (CorpseLootContainerMode == -1)
    {
        const UWorld* World = GetWorld();
        const AWTBRGameState* WTBRGameState = World ? World->GetGameState<AWTBRGameState>() : nullptr;
        bUseCorpseLootContainer = IsValid(WTBRGameState) && WTBRGameState->UsesCorpseLootContainerOnDeath();
    }

    if (bUseCorpseLootContainer)
    {
        UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot death spawn: corpse loot container path selected."));
        SpawnCorpseLootContainer_Internal();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot death spawn: legacy dropped trigger path selected."));
    SpawnLegacyDroppedTriggers_Internal();
}

void UWTBRHealthComponent::SpawnLegacyDroppedTriggers_Internal()
{
    AWTBRCharacter* VictimCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(VictimCharacter) || !VictimCharacter->HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState)
        || !WTBRGameState->IsInMatch()
        || !WTBRGameState->AllowsCorpseLoot()
        || !WTBRGameState->AllowsTriggerPickup())
    {
        return;
    }

    UWTBRTriggerSetComponent* TriggerSetComponent = VictimCharacter->TriggerSetComponent;
    if (!IsValid(TriggerSetComponent))
    {
        return;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> InstalledTriggers;
    TriggerSetComponent->GetInstalledTriggerSlotSnapshots(InstalledTriggers);
    if (InstalledTriggers.Num() == 0)
    {
        return;
    }

    const FVector BaseLocation = VictimCharacter->GetActorLocation();
    const FRotator SpawnRotation = FRotator::ZeroRotator;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = VictimCharacter;
    SpawnParams.Instigator = VictimCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    int32 SpawnedDroppedTriggerCount = 0;
    for (int32 DropIndex = 0; DropIndex < InstalledTriggers.Num(); ++DropIndex)
    {
        const FWTBRInstalledTriggerSlotSnapshot& Snapshot = InstalledTriggers[DropIndex];
        if (!Snapshot.IsValid())
        {
            continue;
        }

        const float AngleRadians = (2.0f * PI * DropIndex) / FMath::Max(1, InstalledTriggers.Num());
        const FVector Offset(FMath::Cos(AngleRadians) * 80.0f, FMath::Sin(AngleRadians) * 80.0f, 20.0f);
        const FVector SpawnLocation = BaseLocation + Offset;

        AWTBRDroppedTriggerActor* DroppedTrigger = World->SpawnActor<AWTBRDroppedTriggerActor>(
            AWTBRDroppedTriggerActor::StaticClass(),
            SpawnLocation,
            SpawnRotation,
            SpawnParams);

        if (!IsValid(DroppedTrigger))
        {
            UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot drop failed for %s slot %d"),
                *GetNameSafe(VictimCharacter),
                Snapshot.SlotIndex);
            continue;
        }

        DroppedTrigger->InitializeDroppedTrigger(Snapshot.DataAsset, Snapshot.SlotIndex, Snapshot.CachedCategory);
        ++SpawnedDroppedTriggerCount;
        UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot dropped trigger for %s slot %d asset %s"),
            *GetNameSafe(VictimCharacter),
            Snapshot.SlotIndex,
            *Snapshot.DataAsset.ToSoftObjectPath().ToString());
    }

    UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot legacy dropped trigger path completed for %s: SpawnedDroppedTriggers=%d"),
        *GetNameSafe(VictimCharacter),
        SpawnedDroppedTriggerCount);
}

void UWTBRHealthComponent::SpawnCorpseLootContainer_Internal()
{
    AWTBRCharacter* VictimCharacter = Cast<AWTBRCharacter>(GetOwner());
    if (!IsValid(VictimCharacter) || !VictimCharacter->HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const AWTBRGameState* WTBRGameState = World->GetGameState<AWTBRGameState>();
    if (!IsValid(WTBRGameState)
        || !WTBRGameState->IsInMatch()
        || !WTBRGameState->AllowsCorpseLoot()
        || !WTBRGameState->AllowsTriggerPickup())
    {
        return;
    }

    UWTBRTriggerSetComponent* TriggerSetComponent = VictimCharacter->TriggerSetComponent;
    if (!IsValid(TriggerSetComponent))
    {
        return;
    }

    TArray<FWTBRInstalledTriggerSlotSnapshot> InstalledTriggers;
    TriggerSetComponent->GetInstalledTriggerSlotSnapshots(InstalledTriggers);
    if (InstalledTriggers.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot container path skipped for %s: no installed trigger entries."),
            *GetNameSafe(VictimCharacter));
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = VictimCharacter;
    SpawnParams.Instigator = VictimCharacter;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AWTBRCorpseLootContainerActor* LootContainer = World->SpawnActor<AWTBRCorpseLootContainerActor>(
        AWTBRCorpseLootContainerActor::StaticClass(),
        VictimCharacter->GetActorLocation(),
        FRotator::ZeroRotator,
        SpawnParams);

    if (!IsValid(LootContainer))
    {
        UE_LOG(LogTemp, Warning, TEXT("WTBR corpse loot container path failed for %s: container spawn returned invalid actor."),
            *GetNameSafe(VictimCharacter));
        return;
    }

    LootContainer->InitializeCorpseLootContainer(InstalledTriggers);
    UE_LOG(LogTemp, Log, TEXT("WTBR corpse loot container path completed for %s: Container=%s Entries=%d"),
        *GetNameSafe(VictimCharacter),
        *GetNameSafe(LootContainer),
        LootContainer->GetLootEntryCount());
}

void UWTBRHealthComponent::DestroyLimb(EWTBRLimbType Limb)
{
    const int32 Idx = (int32)Limb;
    if (Limb == EWTBRLimbType::None || !LimbStates.IsValidIndex(Idx)) return;
    if (LimbStates[Idx].bDestroyed) return;

    FWTBRLimbState& State = LimbStates[Idx];
    State.bDestroyed = true;

    const UWTBRCoreStatsDataAsset* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("DestroyLimb"));
    if (Limb == EWTBRLimbType::LeftLeg || Limb == EWTBRLimbType::RightLeg)
    {
        State.SpeedPenalty  = S ? S->LimbLegSpeedPenalty  : 0.40f;
        State.DamagePenalty = 0.f;
    }
    else
    {
        State.DamagePenalty = S ? S->LimbArmDamagePenalty : 0.35f;
        State.SpeedPenalty  = S ? S->LimbArmSpeedPenalty  : 0.35f;
    }

    const float RateMid = S
        ? (S->LimbHPDrainRateMin + S->LimbHPDrainRateMax) * 0.5f
        : 0.0075f;
    State.HPDrainRateMultiplier = RateMid;

    OnLimbDestroyed.Broadcast(Limb, State);

    if (GetOwnerRole() == ROLE_Authority)
    {
        RefreshLimbDrainTimer();
    }
}

void UWTBRHealthComponent::RestoreLimb(EWTBRLimbType Limb)
{
    const int32 Idx = (int32)Limb;
    if (LimbStates.IsValidIndex(Idx))
    {
        LimbStates[Idx] = FWTBRLimbState();
    }

    // Stop drain timer if no limbs remain destroyed
    if (GetOwnerRole() == ROLE_Authority && GetWorld())
    {
        bool bAnyDestroyed = false;
        for (int32 i = 1; i < LimbStates.Num(); ++i)
        {
            if (LimbStates[i].bDestroyed) { bAnyDestroyed = true; break; }
        }
        if (!bAnyDestroyed)
        {
            GetWorld()->GetTimerManager().ClearTimer(LimbDrainTimerHandle);
        }
    }
}

float UWTBRHealthComponent::GetMaxHP() const
{
    const auto* S = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("GetMaxHP"));
    return S ? S->MaxHP : 300.f;
}

FWTBRLimbState UWTBRHealthComponent::GetLimbState(EWTBRLimbType Limb) const
{
    const int32 Idx = (int32)Limb;
    return LimbStates.IsValidIndex(Idx) ? LimbStates[Idx] : FWTBRLimbState();
}

float UWTBRHealthComponent::GetTotalSpeedPenaltyFromLimbs() const
{
    // Multiplicative: final = 1 - product of (1 - each penalty)
    float Composite = 0.f;
    for (int32 i = 1; i < LimbStates.Num(); ++i)
    {
        if (LimbStates[i].bDestroyed && LimbStates[i].SpeedPenalty > 0.f)
        {
            Composite = 1.f - (1.f - Composite) * (1.f - LimbStates[i].SpeedPenalty);
        }
    }
    return Composite;
}

float UWTBRHealthComponent::GetTotalDamagePenaltyFromLimbs() const
{
    float Composite = 0.f;
    for (int32 i = 1; i < LimbStates.Num(); ++i)
    {
        if (LimbStates[i].bDestroyed && LimbStates[i].DamagePenalty > 0.f)
        {
            Composite = 1.f - (1.f - Composite) * (1.f - LimbStates[i].DamagePenalty);
        }
    }
    return Composite;
}

// ─── Timer Drain ─────────────────────────────────────────────────────────────

void UWTBRHealthComponent::RefreshLimbDrainTimer()
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (!World->GetTimerManager().IsTimerActive(LimbDrainTimerHandle))
    {
        World->GetTimerManager().SetTimer(
            LimbDrainTimerHandle,
            this, &UWTBRHealthComponent::TickLimbDrain,
            LIMB_DRAIN_TICK_INTERVAL,
            true  // looping
        );
    }
}

void UWTBRHealthComponent::TickLimbDrain()
{
    const float MaxHP = GetMaxHP();
    const auto* S     = GetStats();
    LogTest32HealthCoreStats(this, S, TEXT("TickLimbDrain"));
    const float RateMid = S
        ? (S->LimbHPDrainRateMin + S->LimbHPDrainRateMax) * 0.5f
        : 0.0075f;

    float TotalDrain = 0.f;
    for (int32 i = 1; i < LimbStates.Num(); ++i)
    {
        if (LimbStates[i].bDestroyed)
        {
            TotalDrain += RateMid * MaxHP;
        }
    }
    if (TotalDrain > 0.f)
    {
        ApplyDamage(TotalDrain * LIMB_DRAIN_TICK_INTERVAL);
    }
}

// ─── Replication ─────────────────────────────────────────────────────────────

void UWTBRHealthComponent::OnRep_CurrentHP()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[RemoteDamage Test] OnRep_CurrentHP | Target=%s | Auth=%s | Role=%d | CurrentHP=%.1f | MaxHP=%.1f | OnRepFired=true"),
        *GetNameSafe(GetOwner()),
        GetOwner() && GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"),
        GetOwner() ? static_cast<int32>(GetOwner()->GetLocalRole()) : -1,
        CurrentHP,
        GetMaxHP());
    OnHPChanged.Broadcast(CurrentHP, GetMaxHP());
}

void UWTBRHealthComponent::OnRep_CombatState(EWTBRCombatState OldState)
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test27 Client OnRep_CombatState] Owner=%s | NetMode=%d | Role=%d | OldState=%d | NewState=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
        (int32)OldState,
        (int32)CurrentCombatState,
        CurrentHP,
        CurrentDownedHP,
        bIsInvulnerable ? TEXT("true") : TEXT("false"));

    OnCombatStateChanged.Broadcast(CurrentCombatState, OldState);

    if (CurrentCombatState == EWTBRCombatState::Downed)
    {
        OnDowned.Broadcast();
    }
    else if (CurrentCombatState == EWTBRCombatState::Eliminated)
    {
        OnDeath.Broadcast();
    }
}

void UWTBRHealthComponent::OnRep_IsInvulnerable()
{
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Test28 Client OnRep_IsInvulnerable] Owner=%s | NetMode=%d | Role=%d | State=%d | CurrentHP=%.1f | DownedHP=%.1f | Invulnerable=%s"),
        *GetNameSafe(GetOwner()),
        (int32)GetNetMode(),
        GetOwner() ? (int32)GetOwner()->GetLocalRole() : -1,
        (int32)CurrentCombatState,
        CurrentHP,
        CurrentDownedHP,
        bIsInvulnerable ? TEXT("true") : TEXT("false"));

    OnInvulnerabilityChanged.Broadcast(bIsInvulnerable);
}

void UWTBRHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UWTBRHealthComponent, CurrentHP);
    DOREPLIFETIME(UWTBRHealthComponent, CurrentCombatState);
    DOREPLIFETIME(UWTBRHealthComponent, bIsInvulnerable);
    DOREPLIFETIME(UWTBRHealthComponent, CurrentDownedHP);
    DOREPLIFETIME(UWTBRHealthComponent, LimbStates);
}
