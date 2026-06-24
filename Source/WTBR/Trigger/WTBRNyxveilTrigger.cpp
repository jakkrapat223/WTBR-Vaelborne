// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRNyxveilTrigger.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRNyxveilTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRNyxveilParams& Params = DataAsset->NyxveilParams;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(Params.NyxveilVaelCost)) return false;

    CachedScanRadius = Params.NyxveilScanRadius;

    UWorld* World = OwnerCharacter->GetWorld();

    // Immediate first scan
    DoScan();

    // Repeating scan every NyxveilPingInterval
    World->GetTimerManager().SetTimer(
        PingTimerHandle, this, &UWTBRNyxveilTrigger::DoScan,
        Params.NyxveilPingInterval, /*bLoop=*/true);

    // Stop the repeating scan after NyxveilDuration
    World->GetTimerManager().SetTimer(
        StopTimerHandle, this, &UWTBRNyxveilTrigger::StopScan,
        Params.NyxveilDuration, /*bLoop=*/false);

    OnNyxveilActivated();
    return true;
}

void UWTBRNyxveilTrigger::Deactivate_Implementation()
{
    StopScan();
    Super::Deactivate_Implementation();
}

void UWTBRNyxveilTrigger::DoScan()
{
    if (!OwnerCharacter.IsValid()) { StopScan(); return; }
    if (!OwnerCharacter->HasAuthority()) return;

    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World)) { StopScan(); return; }

    const FVector Origin = OwnerCharacter->GetActorLocation();

    TArray<FHitResult> Hits;
    FCollisionObjectQueryParams ObjParams(ECC_Pawn);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());

    World->SweepMultiByObjectType(
        Hits, Origin, Origin, FQuat::Identity,
        ObjParams, FCollisionShape::MakeSphere(CachedScanRadius), QueryParams);

    TSet<AActor*> Detected;
    for (const FHitResult& Hit : Hits)
    {
        if (IsValid(Hit.GetActor()))
            Detected.Add(Hit.GetActor());
    }

    UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>();
    if (PingSys)
    {
        for (AActor* Target : Detected)
            PingSys->RegisterActionPing(Target);
    }
    else
    {
        // TODO: UWTBRActionPingSubsystem not found — scan results will not be reported on radar
        UE_LOG(LogTemp, Warning,
            TEXT("WTBRNyxveilTrigger: UWTBRActionPingSubsystem not available. "
                 "Detected %d actor(s) but radar ping was not dispatched."),
            Detected.Num());
    }

    OnNyxveilDetected(Detected.Num());
}

void UWTBRNyxveilTrigger::StopScan()
{
    if (!OwnerCharacter.IsValid()) return;
    UWorld* World = OwnerCharacter->GetWorld();
    if (!IsValid(World)) return;
    World->GetTimerManager().ClearTimer(PingTimerHandle);
    World->GetTimerManager().ClearTimer(StopTimerHandle);
}
