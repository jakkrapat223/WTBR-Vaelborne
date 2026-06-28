// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRAcervynTrigger.h"
#include "WTBRCharacter.h"
#include "Components/WTBRVaelComponent.h"
#include "Trigger/WTBRTriggerDataAsset.h"
#include "Actors/WTBRProjectileBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UWTBRAcervynTrigger::InitializeTrigger(
    AWTBRCharacter* InOwnerCharacter,
    UWTBRTriggerDataAsset* InDataAsset)
{
    Super::InitializeTrigger(InOwnerCharacter, InDataAsset);

    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] Initialize | Owner=%s | DataAsset=%s | Damage=%.1f | VaelCost=%.1f | Cooldown=%.3f | Range=%.1f | Speed=%.1f | BurstCount=%d | BurstInterval=%.3f | HomingAccel=%.1f | ProjectileClass=%s"),
        *GetNameSafe(InOwnerCharacter),
        *GetNameSafe(InDataAsset),
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynDamage : -1.0f,
        IsValid(InDataAsset) ? InDataAsset->VaelCostPerUse : -1.0f,
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynFireCooldown : -1.0f,
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynRange : -1.0f,
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynSpeed : -1.0f,
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynBurstCount : -1,
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynBurstInterval : -1.0f,
        IsValid(InDataAsset) ? InDataAsset->AcervynParams.AcervynHomingAcceleration : -1.0f,
        IsValid(InDataAsset) ? *GetNameSafe(InDataAsset->AcervynParams.AcervynProjectileClass) : TEXT("None"));
}

bool UWTBRAcervynTrigger::Activate_Implementation(
    const FInputActionValue& InputValue,
    bool bIsDualWield)
{
    const float CurrentVael = OwnerCharacter.IsValid() && IsValid(OwnerCharacter->VaelComponent)
        ? OwnerCharacter->VaelComponent->GetCurrentVael()
        : -1.0f;
    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] Activate Start | Owner=%s | HasAuthority=%s | DualWield=%s | DataAsset=%s | CurrentVael=%.2f | CooldownActive=%s | BurstShotsRemaining=%d"),
        *GetNameSafe(OwnerCharacter.Get()),
        OwnerCharacter.IsValid() && OwnerCharacter->HasAuthority() ? TEXT("true") : TEXT("false"),
        bIsDualWield ? TEXT("true") : TEXT("false"),
        *GetNameSafe(DataAsset),
        CurrentVael,
        IsOnCooldown() ? TEXT("true") : TEXT("false"),
        BurstShotsRemaining);

    if (!OwnerCharacter.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Acervyn Retest] HitRejected | Reason=InvalidOwner"));
        return false;
    }
    if (!OwnerCharacter->HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Acervyn Retest] HitRejected | Reason=NoAuthority | Owner=%s"), *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (!IsValid(DataAsset))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Acervyn Retest] HitRejected | Reason=InvalidDataAsset | Owner=%s"), *GetNameSafe(OwnerCharacter.Get()));
        return false;
    }
    if (IsOnCooldown())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Acervyn Retest] HitRejected | Reason=Cooldown | Owner=%s | Cooldown=%.3f"),
            *GetNameSafe(OwnerCharacter.Get()),
            GetCooldownDuration());
        return false;
    }

    if (!OwnerCharacter->VaelComponent ||
        !OwnerCharacter->VaelComponent->TryConsumeVael(DataAsset->VaelCostPerUse))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Acervyn Retest] VaelConsume | Result=Fail | Owner=%s | Cost=%.2f | CurrentVael=%.2f"),
            *GetNameSafe(OwnerCharacter.Get()),
            DataAsset->VaelCostPerUse,
            CurrentVael);
        return false;
    }
    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] VaelConsume | Result=Success | Owner=%s | Cost=%.2f | OldVael=%.2f | NewVael=%.2f"),
        *GetNameSafe(OwnerCharacter.Get()),
        DataAsset->VaelCostPerUse,
        CurrentVael,
        OwnerCharacter->VaelComponent->GetCurrentVael());

    // Sphere overlap to find the nearest valid target
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());

    const bool bOverlapOK = GetWorld()->OverlapMultiByChannel(
        Overlaps,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(DataAsset->AcervynParams.AcervynRange),
        Params);
    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] MultiHitState | Owner=%s | OverlapOK=%s | RawOverlaps=%d | Range=%.1f | CurrentDesign=NearestSingleTargetBurst"),
        *GetNameSafe(OwnerCharacter.Get()),
        bOverlapOK ? TEXT("true") : TEXT("false"),
        Overlaps.Num(),
        DataAsset->AcervynParams.AcervynRange);

    BurstTarget.Reset();
    float NearestDistSq = FLT_MAX;
    const FVector OwnerLoc = OwnerCharacter->GetActorLocation();

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AWTBRCharacter* Candidate = Cast<AWTBRCharacter>(Overlap.GetActor());
        UE_LOG(LogTemp, Warning,
            TEXT("[Acervyn Retest] HitAttempt | Owner=%s | Candidate=%s | CandidateClass=%s"),
            *GetNameSafe(OwnerCharacter.Get()),
            *GetNameSafe(Overlap.GetActor()),
            IsValid(Overlap.GetActor()) ? *GetNameSafe(Overlap.GetActor()->GetClass()) : TEXT("None"));

        if (!IsValid(Candidate))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Acervyn Retest] HitRejected | Reason=InvalidTarget | Candidate=%s"),
                *GetNameSafe(Overlap.GetActor()));
            continue;
        }
        if (Candidate == OwnerCharacter.Get())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Acervyn Retest] HitRejected | Reason=Self | Candidate=%s"),
                *GetNameSafe(Candidate));
            continue;
        }

        const float DistSq = FVector::DistSquared(OwnerLoc, Candidate->GetActorLocation());
        if (DistSq < NearestDistSq)
        {
            NearestDistSq = DistSq;
            BurstTarget = Candidate;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] HitAccepted | Owner=%s | BurstTarget=%s | Distance=%.1f | Note=NearestTargetSelected"),
        *GetNameSafe(OwnerCharacter.Get()),
        *GetNameSafe(BurstTarget.Get()),
        BurstTarget.IsValid() ? FMath::Sqrt(NearestDistSq) : -1.0f);

    BurstShotsRemaining = DataAsset->AcervynParams.AcervynBurstCount;

    StartCooldown();
    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] CooldownStart | Owner=%s | Cooldown=%.3f | BurstCount=%d | BurstInterval=%.3f"),
        *GetNameSafe(OwnerCharacter.Get()),
        GetCooldownDuration(),
        BurstShotsRemaining,
        DataAsset->AcervynParams.AcervynBurstInterval);

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

    UE_LOG(LogTemp, Warning,
        TEXT("[Acervyn Retest] MultiHitState | Owner=%s | BurstShotsRemainingBefore=%d | Target=%s | ProjectileClass=%s | Damage=%.1f | Speed=%.1f | HomingAccel=%.1f"),
        *GetNameSafe(OwnerCharacter.Get()),
        BurstShotsRemaining,
        *GetNameSafe(BurstTarget.Get()),
        *GetNameSafe(ProjClass),
        Damage,
        Speed,
        HomingAccel);

    AWTBRProjectileBase* Proj = FireProjectile(ProjClass, Damage, Speed, 0.0f, false, 0.0f);

    if (IsValid(Proj) && BurstTarget.IsValid())
    {
        Proj->EnableHoming(BurstTarget.Get()->GetRootComponent(), HomingAccel);
        UE_LOG(LogTemp, Warning,
            TEXT("[Acervyn Retest] HitAccepted | Projectile=%s | HomingTarget=%s | HomingAccel=%.1f"),
            *GetNameSafe(Proj),
            *GetNameSafe(BurstTarget.Get()),
            HomingAccel);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Acervyn Retest] HitRejected | Reason=%s | Projectile=%s | Target=%s"),
            IsValid(Proj) ? TEXT("InvalidTarget") : TEXT("SpawnFailed"),
            *GetNameSafe(Proj),
            *GetNameSafe(BurstTarget.Get()));
    }

    BurstShotsRemaining--;

    if (BurstShotsRemaining <= 0)
    {
        GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
        BurstTarget.Reset();
        UE_LOG(LogTemp, Warning,
            TEXT("[Acervyn Retest] ActionEnd | Owner=%s | Cleanup=BurstTimerCleared_TargetReset"),
            *GetNameSafe(OwnerCharacter.Get()));
    }
}

float UWTBRAcervynTrigger::GetCooldownDuration() const
{
    if (!IsValid(DataAsset)) return 0.5f;
    return DataAsset->AcervynParams.AcervynFireCooldown;
}
