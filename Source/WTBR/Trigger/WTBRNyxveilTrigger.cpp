// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRNyxveilTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "Engine/World.h"

bool UWTBRNyxveilTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp)) return false;
    if (!VaelComp->TryConsumeVael(DataAsset->NyxveilParams.NyxveilVaelCost)) return false;

    const float ScanRadius   = DataAsset->NyxveilParams.NyxveilScanRadius;
    const float Duration     = DataAsset->NyxveilParams.NyxveilDuration;
    const float PingInterval = DataAsset->NyxveilParams.NyxveilPingInterval;

    // Fire first scan immediately — no delay feel
    PerformScan();

    // Repeating scan every PingInterval — NOT Tick
    GetWorld()->GetTimerManager().SetTimer(
        ScanTimer,
        this,
        &UWTBRNyxveilTrigger::PerformScan,
        PingInterval,
        true);

    // Stop the repeating scan after Duration (one-shot)
    GetWorld()->GetTimerManager().SetTimer(
        ScanDurationTimer,
        this,
        &UWTBRNyxveilTrigger::OnScanExpired,
        Duration,
        false);

    // Nyxveil doesn't call FireBlackProjectile — manually fire the VFX event
    OnBlackTriggerFired(OwnerCharacter->GetActorForwardVector());

    StartCooldown();
    return true;
}

void UWTBRNyxveilTrigger::PerformScan()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;
    if (!IsValid(DataAsset)) return;

    const float ScanRadius = DataAsset->NyxveilParams.NyxveilScanRadius;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(ScanRadius),
        QueryParams);

    TArray<AWTBRCharacter*> DetectedChars;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Overlap.GetActor());
        if (!IsValid(Candidate) || Candidate == OwnerCharacter.Get()) continue;

        if (!DetectedChars.Contains(Candidate))
            DetectedChars.Add(Candidate);
    }

    UWTBRActionPingSubsystem* PingSystem =
        GetWorld()->GetSubsystem<UWTBRActionPingSubsystem>();
    if (!IsValid(PingSystem)) return;

    for (AWTBRCharacter* DetectedChar : DetectedChars)
    {
        PingSystem->RegisterActionPing(DetectedChar);
    }
}

void UWTBRNyxveilTrigger::OnScanExpired()
{
    GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
}

void UWTBRNyxveilTrigger::Deactivate_Implementation()
{
    OnScanExpired();
    GetWorld()->GetTimerManager().ClearTimer(ScanDurationTimer);
    Super::Deactivate_Implementation();
}

float UWTBRNyxveilTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 2.0f;
    return DataAsset->NyxveilParams.NyxveilDuration + 5.0f; // ⚠ Playtest
}
