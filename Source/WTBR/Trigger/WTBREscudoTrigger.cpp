// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBREscudoTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

TSubclassOf<AWTBRAegornWallActor> UWTBREscudoTrigger::LoadWallActorClass() const
{
    if (WallActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=null | Reason=WallActorClass null"));
        return nullptr;
    }
    TSubclassOf<AWTBRAegornWallActor> WallClass = WallActorClass.LoadSynchronous();
    if (!IsValid(WallClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=%s | Reason=Load failed"),
            *WallActorClass.ToString());
        return nullptr;
    }
    return WallClass;
}

FRotator UWTBREscudoTrigger::ComputeSurfaceAlignedRotation(
    const FVector& SurfaceNormal, const FVector& ViewDirection)
{
    const FVector GrowAxis = SurfaceNormal.GetSafeNormal();
    if (GrowAxis.IsNearlyZero())
    {
        return FRotator::ZeroRotator;
    }

    // Near-parallel to the grow axis leaves the spin about that axis
    // undetermined, so step down through fallbacks until one is usable.
    // Floor/ceiling normals take the view direction (preserving the old
    // yaw-only feel); a wall normal, which the view is almost always pointing
    // straight into, lands on world up and produces the horizontal jut.
    constexpr float ParallelTolerance = 0.99f;
    FVector DesiredThinAxis = ViewDirection.GetSafeNormal();
    if (DesiredThinAxis.IsNearlyZero()
        || FMath::Abs(FVector::DotProduct(DesiredThinAxis, GrowAxis)) > ParallelTolerance)
    {
        DesiredThinAxis = FVector::UpVector;
        if (FMath::Abs(FVector::DotProduct(DesiredThinAxis, GrowAxis)) > ParallelTolerance)
        {
            DesiredThinAxis = FVector::ForwardVector;
        }
    }

    // MakeFromZX: +Z exactly on the normal, +X as close to DesiredThinAxis as
    // orthogonality allows.
    return FRotationMatrix::MakeFromZX(GrowAxis, DesiredThinAxis).Rotator();
}

bool UWTBREscudoTrigger::ValidatePanelPlacement(
    TSubclassOf<AWTBRAegornWallActor> WallClass, const FVector& PanelLocation,
    const FVector& SurfaceNormal, const FRotator& PanelRotation,
    FVector& OutSurfacePoint) const
{
    if (!IsValid(DataAsset) || !OwnerCharacter.IsValid() || !GetWorld())
    {
        return false;
    }

    const FVector GrowAxis = SurfaceNormal.GetSafeNormal();
    if (GrowAxis.IsNearlyZero())
    {
        return false;
    }

    // Canon: Escudo must sprout from a surface — floor, wall or ceiling alike.
    // The trace runs ALONG the anchor normal rather than straight down, which
    // is what lets a wall- or ceiling-mounted panel find its surface at all.
    // It spans the snap range on BOTH sides of the nominal point so a preset
    // panel offset onto a bump or into a dip still snaps to real geometry
    // instead of being rejected for hovering just off the surface.
    const float SnapRange = DataAsset->EscudoParams.EscudoSurfaceSnapRange;
    FHitResult SurfaceHit;
    FCollisionQueryParams SurfaceParams;
    SurfaceParams.AddIgnoredActor(OwnerCharacter.Get());
    // Object types, not the WorldStatic trace channel — a character capsule
    // blocks that channel and would be treated as a placeable surface, letting
    // panels snap onto people. See the matching note in
    // AWTBRCharacter::ComputeEscudoAnchor.
    FCollisionObjectQueryParams SurfaceObjects;
    SurfaceObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    SurfaceObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    const bool bHasSurface = GetWorld()->LineTraceSingleByObjectType(
        SurfaceHit,
        PanelLocation + GrowAxis * SnapRange,
        PanelLocation - GrowAxis * SnapRange,
        SurfaceObjects,
        SurfaceParams);
    if (!bHasSurface)
    {
        WTBR_VALIDATION_LOG(Log, TEXT("[Escudo Test] SpawnFail | Reason=NoSupportingSurface | Location=%s | Normal=%s | SnapRange=%.1f"),
            *PanelLocation.ToString(),
            *GrowAxis.ToString(),
            SnapRange);
        return false;
    }

    // Publish the surface point as soon as it is found, BEFORE the remaining
    // checks can bail out — the ghost preview needs somewhere on the surface
    // to draw a rejected panel too. Previously an invalid panel left this
    // unset and the caller fell back to the raw aim-height point, which drew
    // the preview floating in mid-air.
    const FVector SurfacePoint = SurfaceHit.ImpactPoint;
    OutSurfacePoint = SurfacePoint;

    // Where the wall's ORIGIN ends up once the eruption settles: half a panel-
    // height out along the grow axis, not along world up, so this holds for a
    // ceiling or wall anchor just as much as a floor. Both remaining checks
    // below need it, so it is resolved once here.
    //
    // ComputeIntendedCollisionExtent, not WallCollision->GetScaledBoxExtent()
    // — the CDO's own WallCollision only ever has the raw C++ constructor
    // default (10,150,150); the mesh-derived auto-fit only ever applied inside
    // a real spawned instance's BeginPlay, which a Class Default Object never
    // runs. Reading GetScaledBoxExtent() here silently ignored whatever size
    // the Blueprint's WallMesh was actually scaled to.
    const AWTBRAegornWallActor* WallCDO =
        IsValid(WallClass) ? WallClass->GetDefaultObject<AWTBRAegornWallActor>() : nullptr;
    const bool bHasWallCDO = IsValid(WallCDO);
    const FVector BoxExtent = bHasWallCDO ? WallCDO->ComputeIntendedCollisionExtent() : FVector::ZeroVector;
    const FVector FinalLocation = SurfacePoint + GrowAxis * BoxExtent.Z;

    // Anti-abuse (owner's own ask): reject placement too close to an existing
    // Escudo/Aegorn wall — stacking walls to fully enclose yourself into a
    // permanent hideout is not allowed. Side-by-side placement beyond the
    // spacing threshold is still fine.
    //
    // Compared CENTRE to CENTRE, and in 3D. Centre-to-centre because an
    // existing wall's ActorLocation is its centre, so measuring from the new
    // panel's SURFACE point instead would compare two different reference
    // heights and inflate every distance by half a wall — on flat ground a
    // panel 200uu away would have measured 325uu and slipped past a 300uu
    // threshold that used to reject it. In 3D because ceilings are valid
    // anchors now, and a ground-plane measure would let a ceiling panel block
    // the floor panel directly beneath it. On flat ground the two centres sit
    // at equal height, so 3D and horizontal agree and the original behaviour
    // is preserved exactly.
    float ClosestWallDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AWTBRAegornWallActor> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It)) continue;
        ClosestWallDistance = FMath::Min(ClosestWallDistance, FVector::Dist(FinalLocation, It->GetActorLocation()));
    }
    if (ClosestWallDistance < DataAsset->EscudoParams.EscudoMinWallSpacing)
    {
        WTBR_VALIDATION_LOG(Log, TEXT("[Escudo Test] SpawnFail | Reason=TooCloseToExistingWall | Distance=%.1f | MinSpacing=%.1f"),
            ClosestWallDistance,
            DataAsset->EscudoParams.EscudoMinWallSpacing);
        return false;
    }

    // NOTE — there is deliberately no "would this trap a character" rejection
    // here any more, for anyone, caster included (owner's call, 2026-07-20).
    //
    // Erupting a panel under someone is the canon Escudo play, not an error
    // (Hyuse launching Ikoma), and standing on your own panel to ride it is a
    // movement tech the owner wants available. Nothing gets trapped because
    // AWTBRAegornWallActor::ApplyEruptionDisplacement now shoves EVERY
    // character the rising wall sweeps through — caster, ally and enemy alike
    // — along the wall's grow axis.
    //
    // History, so this does not get "helpfully" reinstated: a trap check did
    // live here, and it rejected literally every placement in the game for two
    // separate reasons before anyone noticed it was also blocking the slam
    // feature outright. It used the wall's oriented box from a CDO that
    // reported identity scale, collapsing a thin panel into a fat cube (see
    // AWTBRAegornWallActor::ComputeIntendedCollisionExtent), and it queried
    // via OverlapAnyTestByChannel(ECC_Pawn) — which reads ECC_Pawn as a TRACE
    // channel, "is anything here that BLOCKS pawns", matching every floor and
    // wall in the level. A panel's base sits flush against the surface it
    // sprouts from by design, so that query could never come back clean.
    //
    // If some future placement genuinely must be refused, gate it on the
    // displacement failing, not on the overlap existing.
    return true;
}

AWTBRAegornWallActor* UWTBREscudoTrigger::SpawnOnePanel(
    TSubclassOf<AWTBRAegornWallActor> WallClass, const FVector& SurfacePoint, const FRotator& Rotation)
{
    if (!IsValid(WallClass) || !IsValid(DataAsset) || !OwnerCharacter.IsValid() || !GetWorld())
    {
        return nullptr;
    }

    // Surface reference for the eruption: the wall settles half its height out
    // along the grow axis from the traced surface point, and starts fully
    // buried behind that surface so BeginEscudoEruption can animate it rising
    // into place (canon: Escudo "sprouts" from the surface rather than popping
    // in at full height).
    //
    // The grow axis is Rotation's local +Z, which ComputeSurfaceAlignedRotation
    // aligned to the surface normal — so this is world-up for a floor panel,
    // horizontal for a wall panel, and downward for a ceiling panel, without
    // any special-casing here.
    const AWTBRAegornWallActor* WallCDO = WallClass->GetDefaultObject<AWTBRAegornWallActor>();
    // ComputeIntendedCollisionExtent, not WallCollision->GetScaledBoxExtent():
    // on a CDO that box still holds the raw C++ constructor default, so the
    // eruption used to travel a hardcoded 150 regardless of how large the
    // Blueprint's WallMesh actually is — a wall taller than that popped in
    // already half-risen, a shorter one erupted out of the floor and kept going.
    const float HalfHeight = IsValid(WallCDO)
        ? WallCDO->ComputeIntendedCollisionExtent().Z
        : 150.0f;
    const FVector GrowAxis = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Z);
    const FVector FinalLocation = SurfacePoint + GrowAxis * HalfHeight;
    const FVector EruptStartLocation = SurfacePoint - GrowAxis * HalfHeight;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnAttempt | Class=%s | Location=%s | Rotation=%s"),
        *GetNameSafe(WallClass.Get()),
        *EruptStartLocation.ToString(),
        *Rotation.ToString());

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    // AlwaysSpawn (no engine adjust): overlapping pawns/props are displaced
    // by the eruption tick below instead of the wall being nudged off its
    // placement point.
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AWTBRAegornWallActor* Wall =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(
            WallClass, EruptStartLocation, Rotation, Params);

    if (!IsValid(Wall))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=%s"),
            *GetNameSafe(WallClass.Get()));
        return nullptr;
    }

    Wall->InitializeWall(
        DataAsset->EscudoParams.EscudoWallHP,
        DataAsset->EscudoParams.EscudoWallDuration);
    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnSuccess | Wall=%s | Location=%s | Rotation=%s | WallHP=%.1f | MaxWallHP=%.1f | Duration=%.1f | Replicates=%s"),
        *GetNameSafe(Wall),
        *Wall->GetActorLocation().ToString(),
        *Wall->GetActorRotation().ToString(),
        Wall->WallHP,
        Wall->MaxWallHP,
        DataAsset->EscudoParams.EscudoWallDuration,
        Wall->GetIsReplicated() ? TEXT("true") : TEXT("false"));

    Wall->BeginEscudoEruption(
        FinalLocation,
        DataAsset->EscudoParams.EscudoWallBuildTime,
        DataAsset->EscudoParams.EscudoEruptionImpulse,
        DataAsset->EscudoParams.EscudoEruptionClearanceRatio);

    OnWallPlaced.Broadcast();
    OnEscudoWallSpawned(Wall);
    return Wall;
}

bool UWTBREscudoTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    const AWTBRCharacter* OwnerChar = OwnerCharacter.Get();
    const bool bHasAuthority = OwnerChar && OwnerChar->HasAuthority();
    const float Cost = IsValid(DataAsset) ? DataAsset->VaelCostPerUse : 0.0f;
    UWTBRVaelComponent* Vael = OwnerCharacter.IsValid() ? OwnerCharacter->VaelComponent : nullptr;
    const float CurrentVael = IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f;

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] Activate Start | Owner=%s | Authority=%s | CurrentVael=%.2f | Cost=%.2f"),
        *GetNameSafe(OwnerChar),
        bHasAuthority ? TEXT("true") : TEXT("false"),
        CurrentVael,
        Cost);

    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (!IsValid(DataAsset))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] Activate Abort | Owner=%s | Reason=DataAsset invalid"),
            *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }

    // Validate everything spawnable BEFORE consuming Vael — a failed placement
    // must never cost the player anything.
    TSubclassOf<AWTBRAegornWallActor> WallClass = LoadWallActorClass();
    if (!IsValid(WallClass))
    {
        return false;
    }

    // Tap stays a fixed-distance drop in front of the character (owner-locked
    // 2026-07-20: "Tap Spawn ข้างหน้าตัวละคร"). Only Hold/preset mode aims.
    const FVector SpawnLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * 150.0f;

    // Tap anchors to the floor under that spot, so its normal comes from a
    // downward probe rather than the player's aim. On flat ground this
    // resolves to plain world-up and the panel stands exactly as it always
    // has; on a slope it now tilts to match, per the same
    // perpendicular-to-surface canon rule the preset flow follows.
    FVector SurfaceNormal = FVector::UpVector;
    FHitResult FloorHit;
    FCollisionQueryParams FloorParams;
    FloorParams.AddIgnoredActor(OwnerCharacter.Get());
    FCollisionObjectQueryParams FloorObjects;
    FloorObjects.AddObjectTypesToQuery(ECC_WorldStatic);
    FloorObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    if (GetWorld()->LineTraceSingleByObjectType(
            FloorHit, SpawnLoc,
            SpawnLoc - FVector(0.0f, 0.0f, DataAsset->EscudoParams.EscudoSurfaceSnapRange),
            FloorObjects, FloorParams))
    {
        SurfaceNormal = FloorHit.ImpactNormal;
    }

    const FRotator SpawnRot = ComputeSurfaceAlignedRotation(
        SurfaceNormal, OwnerCharacter->GetActorForwardVector());

    FVector SurfacePoint;
    if (!ValidatePanelPlacement(WallClass, SpawnLoc, SurfaceNormal, SpawnRot, SurfacePoint))
    {
        return false;
    }

    if (!IsValid(Vael) || !Vael->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] ConsumeFail | CurrentVael=%.2f | Cost=%.2f"),
            IsValid(Vael) ? Vael->GetCurrentVael() : -1.0f,
            DataAsset->VaelCostPerUse);
        return false;
    }

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] ConsumeSuccess | NewVael=%.2f | Cost=%.2f"),
        Vael->GetCurrentVael(),
        DataAsset->VaelCostPerUse);

    return IsValid(SpawnOnePanel(WallClass, SurfacePoint, SpawnRot));
}
