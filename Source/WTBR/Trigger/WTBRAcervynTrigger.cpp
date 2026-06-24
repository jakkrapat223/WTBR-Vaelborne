// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRAcervynTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"

bool UWTBRAcervynTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset)) return false;

    const FWTBRAcervynParams& Params = DataAsset->AcervynParams;
    if (!Params.AcervynProjectileClass) return false;
    if (Params.AcervynBurstCount <= 0) return false;

    UWorld* World = OwnerCharacter->GetWorld();

    // Server-side trace from camera to find a homing target (same pattern as Venyx)
    FVector CamLoc;
    FRotator CamRot;
    OwnerCharacter->GetActorEyesViewPoint(CamLoc, CamRot);
    const FVector TraceDir = CamRot.Vector();
    const FVector TraceEnd = CamLoc + TraceDir * Params.AcervynRange;

    FHitResult Hit;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(OwnerCharacter.Get());
    const bool bHit = World->LineTraceSingleByChannel(
        Hit, CamLoc, TraceEnd, ECC_Pawn, TraceParams);

    USceneComponent* HomingTarget = nullptr;
    if (bHit && IsValid(Hit.GetActor()))
        HomingTarget = Hit.GetActor()->GetRootComponent();

    const FVector Forward = TraceDir;
    const FVector Right   = OwnerCharacter->GetActorRightVector();
    const FVector BaseOrigin = OwnerCharacter->GetActorLocation() + Forward * 100.0f;

    // Fan spread centred on Forward — 30° total, evenly distributed
    constexpr float TotalSpreadDeg   = 30.0f;
    constexpr float SpawnOffsetPerBullet = 30.0f; // UU right-shift per bullet (avoids first-frame overlap)
    const float StepDeg = (Params.AcervynBurstCount > 1)
        ? TotalSpreadDeg / static_cast<float>(Params.AcervynBurstCount - 1)
        : 0.0f;
    const float StartDeg = -TotalSpreadDeg * 0.5f;

    for (int32 i = 0; i < Params.AcervynBurstCount; ++i)
    {
        const float   SpreadAngle = StartDeg + i * StepDeg;
        const FVector ShotDir     = Forward.RotateAngleAxis(SpreadAngle, FVector::UpVector);
        const FVector SpawnOrigin = BaseOrigin + Right * (i * SpawnOffsetPerBullet);
        const FTransform SpawnTF(ShotDir.Rotation(), SpawnOrigin);

        AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
            Params.AcervynProjectileClass, SpawnTF, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Proj)) continue;

        Proj->MaxRange = Params.AcervynRange;
        Proj->InitializeProjectile(Params.AcervynDamage, Params.AcervynSpeed,
            ETriggerCategory::Gunner, false, false, 0.0f);
        Proj->FinishSpawning(SpawnTF);
        Proj->Launch(ShotDir, OwnerCharacter.Get()); // OwnerInstigator same for all — burst same-fire exempt

        if (IsValid(HomingTarget))
            Proj->EnableHoming(HomingTarget, Params.AcervynHomingAcceleration);
    }

    if (UWTBRActionPingSubsystem* PingSys =
        World->GetSubsystem<UWTBRActionPingSubsystem>())
    {
        PingSys->RegisterActionPing(OwnerCharacter.Get());
    }

    OnAcervynFired();
    return true;
}
