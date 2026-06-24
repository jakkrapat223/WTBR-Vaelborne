// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRKaldrixTrigger.h"
#include "Actors/WTBRKaldrixZone.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"

bool UWTBRKaldrixTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRKaldrixParams& Params = DataAsset->KaldrixParams;
    if (!Params.KaldrixZoneClass) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(Params.KaldrixVaelCost)) return false;

    UWorld* World = OwnerCharacter->GetWorld();

    // LineTrace downward to find the floor placement point
    const FVector TraceStart = OwnerCharacter->GetActorLocation();
    const FVector TraceEnd   = TraceStart - FVector(0.0f, 0.0f, 5000.0f);

    FHitResult Hit;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(OwnerCharacter.Get());

    const bool bHit = World->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_WorldStatic, TraceParams);

    const FVector SpawnLoc = bHit ? Hit.ImpactPoint : TraceEnd;
    const FTransform SpawnTF(FQuat::Identity, SpawnLoc);

    AWTBRKaldrixZone* Zone = World->SpawnActorDeferred<AWTBRKaldrixZone>(
        Params.KaldrixZoneClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Zone)) return false;

    Zone->InitializeZone(Params.KaldrixDamage, Params.KaldrixRadius,
        Params.KaldrixArmTime, Params.KaldrixStaggerDuration, OwnerCharacter.Get());
    Zone->FinishSpawning(SpawnTF);

    if (UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>())
        PingSys->RegisterActionPing(OwnerCharacter.Get());

    OnKaldrixActivated();
    return true;
}
