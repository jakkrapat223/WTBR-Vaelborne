// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBREscudoTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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
    if (WallActorClass.IsNull())
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=null | Reason=WallActorClass null"));
        return false;
    }
    TSubclassOf<AWTBRAegornWallActor> WallClass =
        WallActorClass.LoadSynchronous();
    if (!IsValid(WallClass))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=%s | Reason=Load failed"),
            *WallActorClass.ToString());
        return false;
    }

    const FVector SpawnLoc = OwnerCharacter->GetActorLocation()
        + OwnerCharacter->GetActorForwardVector() * 150.0f;
    const FRotator SpawnRot = OwnerCharacter->GetActorRotation();

    // Canon: Escudo must sprout from a surface — no supporting ground within
    // snap range below the placement point means no wall and no Vael cost.
    FHitResult SurfaceHit;
    FCollisionQueryParams SurfaceParams;
    SurfaceParams.AddIgnoredActor(OwnerCharacter.Get());
    const bool bHasSurface = GetWorld()->LineTraceSingleByChannel(
        SurfaceHit,
        SpawnLoc,
        SpawnLoc - FVector(0.0f, 0.0f, DataAsset->EscudoParams.EscudoSurfaceSnapRange),
        ECC_WorldStatic,
        SurfaceParams);
    if (!bHasSurface)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Reason=NoSupportingSurface | Location=%s | SnapRange=%.1f"),
            *SpawnLoc.ToString(),
            DataAsset->EscudoParams.EscudoSurfaceSnapRange);
        return false;
    }

    // Ground reference for the eruption: previously SpawnLoc (the raw aim
    // point) was used for the actual spawn transform even though the trace
    // above found the real surface — the wall floated at aim height instead
    // of snapping to ground. Use the traced ImpactPoint's Z from here on;
    // X/Y are unchanged since the trace is a straight vertical line.
    const FVector GroundPoint = SurfaceHit.ImpactPoint;
    const AWTBRAegornWallActor* WallCDO = WallClass->GetDefaultObject<AWTBRAegornWallActor>();
    const float HalfHeight = (IsValid(WallCDO) && IsValid(WallCDO->WallCollision))
        ? WallCDO->WallCollision->GetScaledBoxExtent().Z
        : 150.0f;
    const FVector FinalLocation = GroundPoint + FVector(0.0f, 0.0f, HalfHeight);
    const FVector EruptStartLocation = GroundPoint - FVector(0.0f, 0.0f, HalfHeight);

    // Anti-abuse (owner's own ask): reject placement too close (horizontally)
    // to an existing Escudo/Aegorn wall — stacking walls to fully enclose
    // yourself into a permanent hideout is not allowed. Side-by-side placement
    // beyond the spacing threshold is still fine. On-screen "cannot place"
    // feedback is deferred; this only gates server-side for now.
    float ClosestWallDistance = TNumericLimits<float>::Max();
    for (TActorIterator<AWTBRAegornWallActor> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It)) continue;
        ClosestWallDistance = FMath::Min(ClosestWallDistance, FVector::Dist2D(GroundPoint, It->GetActorLocation()));
    }
    if (ClosestWallDistance < DataAsset->EscudoParams.EscudoMinWallSpacing)
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Reason=TooCloseToExistingWall | Distance=%.1f | MinSpacing=%.1f"),
            ClosestWallDistance,
            DataAsset->EscudoParams.EscudoMinWallSpacing);
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

    WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnAttempt | Class=%s | Location=%s | Rotation=%s"),
        *GetNameSafe(WallClass.Get()),
        *EruptStartLocation.ToString(),
        *SpawnRot.ToString());

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    // AlwaysSpawn (no engine adjust): overlapping pawns/props are displaced
    // by the eruption tick below instead of the wall being nudged off its
    // placement point.
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // Spawn fully underground at the traced ground point; BeginEscudoEruption
    // below animates it up to FinalLocation (canon: Escudo "sprouts" from the
    // surface rather than popping in at full height).
    AWTBRAegornWallActor* Wall =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(
            WallClass, EruptStartLocation, SpawnRot, Params);

    if (!IsValid(Wall))
    {
        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] SpawnFail | Class=%s"),
            *GetNameSafe(WallClass.Get()));
        return false;
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
        DataAsset->EscudoParams.EscudoAllyPushImpulse,
        DataAsset->EscudoParams.EscudoEnemyLaunchImpulse);

    OnWallPlaced.Broadcast();
    OnEscudoWallSpawned(Wall);
    return true;
}

