// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRSerpveilTrigger.h"
#include "Actors/WTBRProjectileBase.h"
#include "Trigger/WTBRTriggerSetComponent.h"
#include "Subsystem/WTBRActionPingSubsystem.h"
#include "Components/WTBRVaelComponent.h"
#include "WTBRCharacter.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

// ─── Activate (no-op — fire is RPC-driven on release) ────────────────────────

bool UWTBRSerpveilTrigger::Activate_Implementation(
    const FInputActionValue& /*InputValue*/,
    bool /*bIsDualWield*/)
{
    // Serpveil fires on button release via Server_FireSerpveil, not here.
    return true;
}

// ─── OnTriggerActivated — client starts charge tracking ──────────────────────

void UWTBRSerpveilTrigger::OnTriggerActivated_Implementation(
    AActor* /*OwnerActor*/, bool bIsMain)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->IsLocallyControlled()) return;

    bCachedIsMain   = bIsMain;
    ChargeStartTime = OwnerCharacter->GetWorld()->GetTimeSeconds();

    OwnerCharacter->GetWorld()->GetTimerManager().SetTimer(
        ChargeUpdateTimer,
        this, &UWTBRSerpveilTrigger::UpdateChargeProgress,
        0.05f, /*bLoop=*/true);

    OnSerpveilChargeStart();
}

// ─── OnTriggerDeactivated — client sends charge result to server ──────────────

void UWTBRSerpveilTrigger::OnTriggerDeactivated_Implementation(
    AActor* /*OwnerActor*/, bool bIsMain)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->IsLocallyControlled()) return;

    StopChargeTracking();

    if (!IsValid(DataAsset)) return;
    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;

    const float Elapsed     = OwnerCharacter->GetWorld()->GetTimeSeconds() - ChargeStartTime;
    const float ChargeFrac  = FMath::Clamp(Elapsed / FMath::Max(Params.SerpveilChargeTime, 0.01f), 0.0f, 1.0f);
    const float FinalRange  = FMath::Lerp(Params.SerpveilMinRange, Params.SerpveilMaxRange, ChargeFrac);

    FVector  EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);

    if (UWTBRTriggerSetComponent* TrigComp = OwnerCharacter->TriggerSetComponent)
    {
        TrigComp->Server_FireSerpveil(Params.PresetShape, EyeRot, FinalRange, bIsMain);
    }
}

// ─── ExecuteServerFire — server validates Vael and spawns projectile ──────────

void UWTBRSerpveilTrigger::ExecuteServerFire(
    EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange)
{
    if (!OwnerCharacter.IsValid()) return;
    if (!OwnerCharacter->HasAuthority()) return;
    if (!IsValid(DataAsset)) return;

    const FWTBRSerpveilParams& Params = DataAsset->SerpveilParams;
    if (!Params.SerpveilProjectileClass) return;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp)) return;

    // Vael validation: how far can we afford based on remaining Vael?
    const float MaxAffordableRange =
        (VaelComp->GetCurrentVael() / Params.SerpveilVaelPerSecond)
        * (Params.SerpveilMaxRange - Params.SerpveilMinRange)
        + Params.SerpveilMinRange;

    const float ValidatedRange = FMath::Clamp(
        ChargedRange, Params.SerpveilMinRange, MaxAffordableRange);

    // Consume Vael proportional to validated range
    const float RangeSpan = Params.SerpveilMaxRange - Params.SerpveilMinRange;
    if (RangeSpan > 0.0f)
    {
        const float RangeFraction = (ValidatedRange - Params.SerpveilMinRange) / RangeSpan;
        const float VaelToConsume = RangeFraction * Params.SerpveilVaelPerSecond;
        if (!VaelComp->TryConsumeVael(VaelToConsume)) return;
    }

    // Build world-space path
    FVector  EyeLoc;
    FRotator EyeRot;
    OwnerCharacter->GetActorEyesViewPoint(EyeLoc, EyeRot);
    const FVector SpawnOrigin = EyeLoc + FRotationMatrix(Direction).GetScaledAxis(EAxis::X) * 100.0f;

    TArray<FVector> Points = BuildPathPoints(Shape, SpawnOrigin, Direction, ValidatedRange);
    if (Points.Num() < 2) return;

    const FTransform SpawnTF(Direction, SpawnOrigin);
    UWorld* World = OwnerCharacter->GetWorld();

    AWTBRProjectileBase* Proj = World->SpawnActorDeferred<AWTBRProjectileBase>(
        Params.SerpveilProjectileClass, SpawnTF, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!IsValid(Proj)) return;

    Proj->InitializeProjectile(Params.SerpveilDamage, Params.SerpveilSpeed,
        ETriggerCategory::Gunner, false, false, 0.0f);
    Proj->FinishSpawning(SpawnTF);
    Proj->InitializePathMovement(Points, Params.SerpveilSpeed, OwnerCharacter.Get());

    if (UWTBRActionPingSubsystem* PingSys = World->GetSubsystem<UWTBRActionPingSubsystem>())
        PingSys->RegisterActionPing(OwnerCharacter.Get());

    OnSerpveilFired(Shape);
}

// ─── GetPreviewPathPoints (client only, for aim preview Blueprint) ────────────

TArray<FVector> UWTBRSerpveilTrigger::GetPreviewPathPoints(
    FVector StartLocation, FRotator Direction, float Range) const
{
    if (!IsValid(DataAsset)) return {};
    return BuildPathPoints(
        DataAsset->SerpveilParams.PresetShape, StartLocation, Direction, Range);
}

// ─── BuildPathPoints ──────────────────────────────────────────────────────────

TArray<FVector> UWTBRSerpveilTrigger::BuildPathPoints(
    EWTBRSerpveilShape Shape, FVector StartLocation, FRotator Direction, float Range,
    bool bMirrorY)
{
    const float R = Range;
    TArray<FVector> LocalPts;

    switch (Shape)
    {
    case EWTBRSerpveilShape::Curve:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.5f,  0.0f,      R*0.2f),
            FVector(R,       0.0f,      0.0f),
        };
        break;

    case EWTBRSerpveilShape::SCurve:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.3f,  R*0.2f,    0.0f),
            FVector(R*0.7f,  -R*0.2f,   0.0f),
            FVector(R,       0.0f,      0.0f),
        };
        break;

    case EWTBRSerpveilShape::Hook:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.5f,  0.0f,      0.0f),
            FVector(R*0.8f,  R*0.3f,    0.0f),
            FVector(R,       R*0.5f,    0.0f),
        };
        break;

    case EWTBRSerpveilShape::Spiral:
        for (int32 i = 0; i < 5; ++i)
        {
            const float Angle  = FMath::DegreesToRadians(90.0f * i);
            const float Radius = R * i * 0.25f;
            LocalPts.Add(FVector(
                Radius * FMath::Cos(Angle),
                Radius * FMath::Sin(Angle),
                0.0f));
        }
        break;

    case EWTBRSerpveilShape::Boomerang:
        LocalPts = {
            FVector(0.0f,    0.0f,      0.0f),
            FVector(R*0.5f,  R*0.4f,    0.0f),
            FVector(R,       0.0f,      0.0f),
            FVector(R*0.5f,  -R*0.4f,   0.0f),
            FVector(0.0f,    0.0f,      0.0f),
        };
        break;

    default:
        LocalPts = { FVector(0.0f, 0.0f, 0.0f), FVector(R, 0.0f, 0.0f) };
        break;
    }

    // Mirror Core B: flip Y of every local point before world-space transform
    if (bMirrorY)
    {
        for (FVector& Pt : LocalPts)
            Pt.Y = -Pt.Y;
    }

    // Transform local waypoints to world space via the fire direction
    const FRotationMatrix RotMat(Direction);
    TArray<FVector> WorldPts;
    WorldPts.Reserve(LocalPts.Num());
    for (const FVector& Local : LocalPts)
        WorldPts.Add(StartLocation + RotMat.TransformVector(Local));

    return WorldPts;
}

// ─── Charge progress helpers ──────────────────────────────────────────────────

void UWTBRSerpveilTrigger::UpdateChargeProgress()
{
    if (!OwnerCharacter.IsValid() || !IsValid(DataAsset)) return;

    const float Elapsed    = OwnerCharacter->GetWorld()->GetTimeSeconds() - ChargeStartTime;
    const float MaxTime    = FMath::Max(DataAsset->SerpveilParams.SerpveilChargeTime, 0.01f);
    const float ChargePct  = FMath::Clamp(Elapsed / MaxTime, 0.0f, 1.0f);

    OnSerpveilCharging(ChargePct);

    if (ChargePct >= 1.0f)
        StopChargeTracking();
}

void UWTBRSerpveilTrigger::StopChargeTracking()
{
    if (!OwnerCharacter.IsValid()) return;
    OwnerCharacter->GetWorld()->GetTimerManager().ClearTimer(ChargeUpdateTimer);
}
