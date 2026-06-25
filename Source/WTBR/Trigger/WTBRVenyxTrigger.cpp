// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRVenyxTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

bool UWTBRVenyxTrigger::Activate_Implementation(
    const FInputActionValue& InputValue, bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    if (IsValid(OwnerCharacter->VaelComponent))
    {
        if (!OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
            return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->VenyxParams.VenyxProjectileClass;
    const float Damage        = DataAsset->VenyxParams.VenyxDamage;
    const float Speed         = DataAsset->VenyxParams.VenyxSpeed;
    const float HomingAccel   = DataAsset->VenyxParams.VenyxHomingAcceleration;
    const float SearchRadius  = DataAsset->VenyxParams.VenyxRange;

    AWTBRProjectileBase* SpawnedProj =
        FireProjectile(ProjClass, Damage, Speed, 0.0f, false, 0.0f);

    StartCooldown();

    if (!IsValid(SpawnedProj)) return true;

    // Sphere overlap to find the nearest valid target on the server
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(SearchRadius),
        Params);

    AWTBRCharacter* TargetCharacter = nullptr;
    float NearestDistSq = FLT_MAX;
    const FVector OwnerLoc = OwnerCharacter->GetActorLocation();

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Overlap.GetActor());
        if (!IsValid(Candidate) || Candidate == OwnerCharacter.Get()) continue;
        // Note: team filter to be added Phase 5

        const float DistSq = FVector::DistSquared(OwnerLoc, Candidate->GetActorLocation());
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            TargetCharacter = Candidate;
        }
    }

    if (IsValid(TargetCharacter))
    {
        SpawnedProj->EnableHoming(TargetCharacter->GetRootComponent(), HomingAccel);
    }

    return true;
}

float UWTBRVenyxTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->VenyxParams.VenyxFireCooldown;
}
