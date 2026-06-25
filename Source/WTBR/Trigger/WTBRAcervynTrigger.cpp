// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRAcervynTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

bool UWTBRAcervynTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    if (!OwnerCharacter->VaelComponent ||
        !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        return false;
    }

    // Sphere overlap to find the nearest valid target
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(DataAsset->AcervynParams.AcervynRange),
        Params);

    BurstTarget.Reset();
    float NearestDistSq = FLT_MAX;
    const FVector OwnerLoc = OwnerCharacter->GetActorLocation();

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Overlap.GetActor());
        if (!IsValid(Candidate) || Candidate == OwnerCharacter.Get()) continue;

        const float DistSq = FVector::DistSquared(OwnerLoc, Candidate->GetActorLocation());
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            BurstTarget = Candidate;
        }
    }

    BurstShotsRemaining = DataAsset->AcervynParams.AcervynBurstCount;

    StartCooldown();

    // FIX: Fire the first shot immediately to prevent delay feel
    FireNextBurstShot();

    // Start timer for the remaining shots (if any)
    if (BurstShotsRemaining > 0)
    {
        GetWorld()->GetTimerManager().SetTimer(
            BurstTimer,
            this,
            &UWTBRAcervynTrigger::FireNextBurstShot,
            DataAsset->AcervynParams.AcervynBurstInterval,
            true);
    }

    return true;
}

void UWTBRAcervynTrigger::FireNextBurstShot()
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return;
    if (!IsValid(DataAsset)) return;

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->AcervynParams.AcervynProjectileClass;
    const float Damage      = DataAsset->AcervynParams.AcervynDamage;
    const float Speed       = DataAsset->AcervynParams.AcervynSpeed;
    const float HomingAccel = DataAsset->AcervynParams.AcervynHomingAcceleration;

    AWTBRProjectileBase* Proj = FireProjectile(ProjClass, Damage, Speed, 0.0f, false, 0.0f);

    if (IsValid(Proj) && BurstTarget.IsValid())
    {
        Proj->EnableHoming(BurstTarget.Get()->GetRootComponent(), HomingAccel);
    }

    BurstShotsRemaining--;

    if (BurstShotsRemaining <= 0)
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
        BurstTarget.Reset();
    }
}

float UWTBRAcervynTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->AcervynParams.AcervynFireCooldown;
}
