// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRFulgornTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"

bool UWTBRFulgornTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    if (!OwnerCharacter.IsValid() || !OwnerCharacter->HasAuthority()) return false;
    if (IsOnCooldown() || !IsValid(DataAsset)) return false;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) ||
        !VaelComp->TryConsumeVael(DataAsset->FulgornParams.FulgornVaelCost))
    {
        return false;
    }

    const TSubclassOf<AWTBRProjectileBase> ProjClass = DataAsset->FulgornParams.FulgornProjectileClass;
    const float Damage      = DataAsset->FulgornParams.FulgornDamage;
    const float Speed       = DataAsset->FulgornParams.FulgornProjectileSpeed;
    const float LockRadius  = DataAsset->FulgornParams.FulgornLockRadius;
    const int32 MaxTargets  = DataAsset->FulgornParams.FulgornMaxTargets;
    const float HomingAccel = DataAsset->FulgornParams.FulgornHomingAcceleration;

    // Sphere overlap to find nearby pawns within lock radius
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter.Get());

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(LockRadius),
        QueryParams);

    // Collect unique targets up to MaxTargets
    TArray<AWTBRCharacter*> Targets;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Overlap.GetActor());
        if (!IsValid(Candidate) || Candidate == OwnerCharacter.Get()) continue;

        // CRITICAL: prevent duplicate locking onto same character's multiple hitboxes
        if (!Targets.Contains(Candidate))
        {
            Targets.Add(Candidate);
            if (Targets.Num() >= MaxTargets) break;
        }
    }

    // Spawn one homing projectile per unique target
    for (AWTBRCharacter* Target : Targets)
    {
        AWTBRProjectileBase* Proj = FireBlackProjectile(ProjClass, Damage, Speed, 0.0f, 1);
        if (IsValid(Proj))
        {
            Proj->EnableHoming(Target->GetRootComponent(), HomingAccel);
        }
    }

    StartCooldown();
    return true;
}

float UWTBRFulgornTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 2.0f;
    return DataAsset->FulgornParams.FulgornFireCooldown;
}
