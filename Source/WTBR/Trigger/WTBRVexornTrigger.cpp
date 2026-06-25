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
    return false;
}

void UWTBRVexornTrigger::OnEquipped()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

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
}

void UWTBRVexornTrigger::OnUnequipped()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    bSuppressionActive = false;
    OnRep_bSuppressionActive();

    GetWorld()->GetTimerManager().ClearTimer(TimerHandle_SuppressionPulse);
    RemoveSuppressionFromAll();
}

void UWTBRVexornTrigger::TickSuppression()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;
    if (!IsValid(DataAsset)) return;

    const float Radius = DataAsset->VexornParams.VexornSuppressionRadius;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());

    GetWorld()->OverlapMultiByChannel(
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
        GetWorld()->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!IsValid(PingSys)) return;

    // Unsuppress characters that have left the radius
    for (const TWeakObjectPtr<AWTBRCharacter>& OldTarget : TrackedSuppressed)
    {
        if (!OldTarget.IsValid()) continue;
        if (!CurrentlyOverlapping.Contains(OldTarget))
            PingSys->UnregisterSignalBlock(OldTarget.Get());
    }

    // Suppress characters that have entered the radius
    for (const TWeakObjectPtr<AWTBRCharacter>& NewTarget : CurrentlyOverlapping)
    {
        if (!NewTarget.IsValid()) continue;
        if (!TrackedSuppressed.Contains(NewTarget))
            PingSys->RegisterSignalBlock(NewTarget.Get());
    }

    TrackedSuppressed = CurrentlyOverlapping;
}

void UWTBRVexornTrigger::RemoveSuppressionFromAll()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;

    UWTBRActionPingSubsystem* PingSys =
        GetWorld()->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!IsValid(PingSys)) return;

    for (const TWeakObjectPtr<AWTBRCharacter>& Target : TrackedSuppressed)
    {
        if (Target.IsValid())
            PingSys->UnregisterSignalBlock(Target.Get());
    }
    TrackedSuppressed.Empty();
}
