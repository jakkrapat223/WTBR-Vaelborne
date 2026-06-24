// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVenyxTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRVenyxTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRVenyxParams& Params = DataAsset->VenyxParams;
    if (!Params.VenyxProjectileClass) return false;

    UWorld* World = OwnerCharacter->GetWorld();

    // Server-side trace from camera to find a homing target
    FVector CamLoc;
    FRotator CamRot;
    OwnerCharacter->GetActorEyesViewPoint(CamLoc, CamRot);
    const FVector TraceDir = CamRot.Vector();
    const FVector TraceEnd = CamLoc + TraceDir * Params.VenyxRange;

    FHitResult Hit;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(OwnerCharacter.Get());
    const bool bHit = World->LineTraceSingleByChannel(
        Hit, CamLoc, TraceEnd, ECC_Pawn, TraceParams);

    // Always spawn and fire — homing is optional based on trace result
    const FVector Origin = OwnerCharacter->GetActorLocation() + TraceDir * 100.0f;
    const FTransform SpawnTF(TraceDir.Rotation(), Origin);

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.VenyxProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return false;

    Proj->MaxRange = Params.VenyxRange;
    Proj->InitializeProjectile(Params.VenyxDamage, Params.VenyxSpeed,
        ETriggerCategory::Gunner, false, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->Launch(TraceDir, OwnerCharacter.Get());

    // Enable homing only if trace found a valid character target
    if (bHit && IsValid(Hit.GetActor()))
    {
        if (USceneComponent* TargetRoot = Hit.GetActor()->GetRootComponent())
        {
            Proj->EnableHoming(TargetRoot, Params.VenyxHomingAcceleration);
            OnVenyxHoming();
        }
    }

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnVenyxFired();
    return true;
}
