// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBREscudoTrigger.h"
#include "WTBRValidationLog.h"
#include "Actors/WTBRAegornWallActor.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

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
        *SpawnLoc.ToString(),
        *SpawnRot.ToString());

    FActorSpawnParameters Params;
    Params.Owner      = OwnerCharacter.Get();
    Params.Instigator = OwnerCharacter.Get();
    // AlwaysSpawn (no engine adjust): overlapping pawns are displaced below
    // instead of the wall being nudged off its placement point.
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AWTBRAegornWallActor* Wall =
        GetWorld()->SpawnActor<AWTBRAegornWallActor>(
            WallClass, SpawnLoc, SpawnRot, Params);

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

    DisplaceOverlappingCharacters(Wall);

    OnWallPlaced.Broadcast();
    OnEscudoWallSpawned(Wall);
    return true;
}

// Canon (Hyuse): the erupting wall shoves whoever stands on its footprint —
// teammates get pushed to safety behind the wall (caster side), enemies get
// launched skyward by the eruption force. Displacement only, damage = 0
// (Escudo Slam lock).
void UWTBREscudoTrigger::DisplaceOverlappingCharacters(AWTBRAegornWallActor* Wall)
{
    if (!OwnerCharacter.IsValid() || !IsValid(Wall) || !GetWorld()) return;

    const FVector WallExtent = IsValid(Wall->WallCollision)
        ? Wall->WallCollision->GetScaledBoxExtent()
        : FVector(20.0f, 150.0f, 150.0f);

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());
    QueryParams.AddIgnoredActor(Wall);
    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Wall->GetActorLocation(),
        Wall->GetActorQuat(),
        ECC_Pawn,
        FCollisionShape::MakeBox(WallExtent),
        QueryParams);

    FVector TowardOwner = OwnerCharacter->GetActorLocation() - Wall->GetActorLocation();
    TowardOwner.Z = 0.0f;
    TowardOwner = TowardOwner.IsNearlyZero()
        ? -OwnerCharacter->GetActorForwardVector()
        : TowardOwner.GetSafeNormal();

    const float AllyPush    = DataAsset->EscudoParams.EscudoAllyPushImpulse;
    const float EnemyLaunch = DataAsset->EscudoParams.EscudoEnemyLaunchImpulse;

    TSet<AWTBRCharacter*> Displaced;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Char = Cast<AWTBRCharacter>(Overlap.GetActor());
        if (!IsValid(Char) || Char == OwnerCharacter.Get()) continue;
        if (Displaced.Contains(Char)) continue;
        Displaced.Add(Char);

        const bool bAlly = Char->IsSameTeamAs(OwnerCharacter.Get());
        if (bAlly)
        {
            // Small vertical pop so ground friction doesn't eat the push.
            Char->LaunchCharacter(
                TowardOwner * AllyPush + FVector(0.0f, 0.0f, AllyPush * 0.25f),
                true, true);
        }
        else
        {
            Char->LaunchCharacter(FVector(0.0f, 0.0f, EnemyLaunch), false, true);
        }

        WTBR_VALIDATION_LOG(Verbose, TEXT("[Escudo Test] Displacement | Owner=%s | Target=%s | Relation=%s | Impulse=%.1f"),
            *GetNameSafe(OwnerCharacter.Get()),
            *GetNameSafe(Char),
            bAlly ? TEXT("Ally->PushBehind") : TEXT("Enemy->LaunchUp"),
            bAlly ? AllyPush : EnemyLaunch);
    }
}
