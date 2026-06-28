// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVexornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "Engine/World.h"

bool UWTBRVexornTrigger::Activate_Implementation(
    const FInputActionValue& /*InputValue*/,
    bool /*bIsDualWield*/)
{
    // Passive trigger — suppression is managed by OnEquipped/OnUnequipped, not by button press.
    const UWTBRVaelComponent* Vael = OwnerCharacter.IsValid()
        ? OwnerCharacter->VaelComponent
        : nullptr;
    const float CurrentVael = IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f;
    const float VexornCost = IsValid(DataAsset)
        ? DataAsset->VexornParams.VexornVaelCost
        : 0.0f;

    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] Activate Start | Owner=%s | HasAuthority=%s | Passive=true | TrapSpawn=false | SphereOverlap=true | SignalBlock=true | CurrentVael=%.2f | VaelCost=%.2f | Cooldown=None"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        CurrentVael,
        VexornCost);
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] Activate NoOp | Reason=PassiveManagedByEquip | Returns=false"));
    return false;
}

void UWTBRVexornTrigger::OnReleased_Implementation(
    const FInputActionValue& /*InputValue*/,
    bool /*bIsDualWield*/)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] Release | Owner=%s | HasAuthority=%s | Passive=true | Action=NoOp"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"));
}

void UWTBRVexornTrigger::Deactivate_Implementation()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] Deactivate | Owner=%s | HasAuthority=%s | SuppressionActive=%s | Tracked=%d | Action=NoOp"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bSuppressionActive ? TEXT("true") : TEXT("false"),
        TrackedSuppressed.Num());
}

void UWTBRVexornTrigger::OnEquipped()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] Equipped | Owner=%s | HasAuthority=%s | Passive=true | TrapSpawn=false | SphereOverlap=true | SignalBlock=true"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"));

    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=OwnerInvalid | Function=OnEquipped"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=NoAuthority | Function=OnEquipped"));
        return;
    }
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=WorldInvalid | Function=OnEquipped"));
        return;
    }

    bSuppressionActive = true;
    OnRep_bSuppressionActive();

    // Immediate first pulse — no felt delay on equip
    TickSuppression();

    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle_SuppressionPulse,
        this,
        &UWTBRVexornTrigger::TickSuppression,
        0.5f,
        true);

    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] TimerStart | Owner=%s | Interval=0.500 | SuppressionActive=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        bSuppressionActive ? TEXT("true") : TEXT("false"));
}

void UWTBRVexornTrigger::OnUnequipped()
{
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] Unequipped | Owner=%s | HasAuthority=%s | SuppressionActive=%s | Tracked=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bSuppressionActive ? TEXT("true") : TEXT("false"),
        TrackedSuppressed.Num());

    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=OwnerInvalid | Function=OnUnequipped"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=NoAuthority | Function=OnUnequipped"));
        return;
    }
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=WorldInvalid | Function=OnUnequipped"));
        return;
    }

    bSuppressionActive = false;
    OnRep_bSuppressionActive();

    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SuppressionPulse);
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] TimerStop | Owner=%s | SuppressionActive=%s"),
        *GetNameSafe(OwnerCharacter.Get()),
        bSuppressionActive ? TEXT("true") : TEXT("false"));
    RemoveSuppressionFromAll();
}

void UWTBRVexornTrigger::TickSuppression()
{
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=OwnerInvalid | Function=TickSuppression"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=NoAuthority | Function=TickSuppression"));
        return;
    }
    if (!IsValid(DataAsset))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=DataAssetInvalid | Function=TickSuppression"));
        return;
    }
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=WorldInvalid | Function=TickSuppression"));
        return;
    }

    const float Radius = DataAsset->VexornParams.VexornSuppressionRadius;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());

    World->OverlapMultiByChannel(
        Overlaps,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(Radius),
        QueryParams);

    // Build deduplicated list of valid enemy characters currently in range
    TArray<TWeakObjectPtr<AWTBRCharacter>> CurrentlyOverlapping;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Overlap.GetActor());
        if (!IsValid(Candidate) || Candidate == OwnerCharacter.Get()) continue;

        TWeakObjectPtr<AWTBRCharacter> WeakCandidate(Candidate);
        if (!CurrentlyOverlapping.Contains(WeakCandidate))
            CurrentlyOverlapping.Add(WeakCandidate);
    }

    UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!IsValid(PingSys))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=PingSubsystemInvalid | Function=TickSuppression"));
        return;
    }

    const float CurrentTime = World->GetTimeSeconds();
    const bool bCountsChanged =
        LastRawOverlapCount != Overlaps.Num()
        || LastTargetCount != CurrentlyOverlapping.Num();
    if (bCountsChanged || CurrentTime - LastPulseLogTime >= 2.0f)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Vexorn Test] OverlapPulse | Owner=%s | Radius=%.1f | RawOverlaps=%d | ValidTargets=%d | TrackedBefore=%d | SuppressionActive=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            Radius,
            Overlaps.Num(),
            CurrentlyOverlapping.Num(),
            TrackedSuppressed.Num(),
            bSuppressionActive ? TEXT("true") : TEXT("false"));
        LastPulseLogTime = CurrentTime;
        LastRawOverlapCount = Overlaps.Num();
        LastTargetCount = CurrentlyOverlapping.Num();
    }

    // Unsuppress characters that have left the radius
    for (const TWeakObjectPtr<AWTBRCharacter>& OldTarget : TrackedSuppressed)
    {
        if (!OldTarget.IsValid()) continue;
        if (!CurrentlyOverlapping.Contains(OldTarget))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Vexorn Test] SignalBlockRemoved | Owner=%s | Target=%s | Reason=LeftRadius"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(OldTarget.Get()));
            PingSys->UnregisterSignalBlock(OldTarget.Get());
        }
    }

    // Suppress characters that have entered the radius
    for (const TWeakObjectPtr<AWTBRCharacter>& NewTarget : CurrentlyOverlapping)
    {
        if (!NewTarget.IsValid()) continue;
        if (!TrackedSuppressed.Contains(NewTarget))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Vexorn Test] TargetDetected | Owner=%s | Target=%s | Radius=%.1f"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(NewTarget.Get()),
                Radius);
            UE_LOG(LogTemp, Warning,
                TEXT("[Vexorn Test] SignalBlockApplied | Owner=%s | Target=%s"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(NewTarget.Get()));
            PingSys->RegisterSignalBlock(NewTarget.Get());
        }
    }

    TrackedSuppressed = CurrentlyOverlapping;
}

void UWTBRVexornTrigger::RemoveSuppressionFromAll()
{
    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=OwnerInvalid | Function=RemoveSuppressionFromAll"));
        return;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=NoAuthority | Function=RemoveSuppressionFromAll"));
        return;
    }
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=WorldInvalid | Function=RemoveSuppressionFromAll"));
        return;
    }

    UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!IsValid(PingSys))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Vexorn Test] Fail | Reason=PingSubsystemInvalid | Function=RemoveSuppressionFromAll"));
        return;
    }

    for (const TWeakObjectPtr<AWTBRCharacter>& Target : TrackedSuppressed)
    {
        if (Target.IsValid())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Vexorn Test] SignalBlockRemoved | Owner=%s | Target=%s | Reason=CleanupAll"),
                *GetNameSafe(OwnerCharacter.Get()),
                *GetNameSafe(Target.Get()));
            PingSys->UnregisterSignalBlock(Target.Get());
        }
    }
    UE_LOG(LogTemp, Warning,
        TEXT("[Vexorn Test] CleanupComplete | Owner=%s | RemovedCount=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        TrackedSuppressed.Num());
    TrackedSuppressed.Empty();
}
