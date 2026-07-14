// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#include "Trigger/WTBRMantornTrigger.h"
#include "WTBRCharacter.h"
#include "WTBRValidationLog.h"
#include "Components/WTBRHealthComponent.h"
#include "Components/WTBRVaelComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"

bool UWTBRMantornTrigger::CanPerformInFormAction() const
{
    if (!OwnerCharacter.IsValid()) return false;
    if (!OwnerCharacter->HasAuthority()) return false;
    if (bIsOnCooldown) return false;
    if (!IsValid(DataAsset)) return false;
    return true;
}

bool UWTBRMantornTrigger::WhipSlash()
{
    if (!CanPerformInFormAction()) return false;

    const FWTBRMantornParams& M = DataAsset->MantornParams;

    // Vael charged per swing (hit or miss), mirroring Serpveil per-shot cost.
    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(M.WhipVaelCost))
    {
        return false;
    }

    TArray<FHitResult> Hits;
    PerformWhipSweep(Hits);
    FilterHits(Hits, OwnerCharacter.Get());

    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar) || !IsValid(HitChar->HealthComponent)) continue;
        HitChar->HealthComponent->ApplyDamage(M.WhipDamage, OwnerCharacter.Get());
        OnMantornWhipHit(Hit, M.WhipDamage);
    }

    // Client VFX: bind OnMeleeSwing in Blueprint (broadcasts to all instances).
    OnMeleeSwing.Broadcast(true);

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Orange,
            FString::Printf(TEXT("[Mantorn] Whip — %d hit(s)"), Hits.Num()));
    }

    if (GetWorld())
    {
        bIsOnCooldown = true;
        GetWorld()->GetTimerManager().SetTimer(
            CooldownTimer, this, &UWTBRMantornTrigger::OnCooldownExpired,
            M.ActionCooldown, false);
    }
    return true;
}

bool UWTBRMantornTrigger::SpinSlash()
{
    if (!CanPerformInFormAction()) return false;

    const FWTBRMantornParams& M = DataAsset->MantornParams;
    const float SpinDamage = DataAsset->MeleeHitbox.MantornSpinDamage;

    UWTBRVaelComponent* VaelComp = OwnerCharacter->VaelComponent;
    if (!IsValid(VaelComp) || !VaelComp->TryConsumeVael(M.SpinVaelCost))
    {
        return false;
    }

    TArray<FHitResult> Hits;
    PerformSpinSweep(Hits);
    FilterHits(Hits, OwnerCharacter.Get());

    for (const FHitResult& Hit : Hits)
    {
        AWTBRCharacter* HitChar = Cast<AWTBRCharacter>(Hit.GetActor());
        if (!IsValid(HitChar) || !IsValid(HitChar->HealthComponent)) continue;
        HitChar->HealthComponent->ApplyDamage(SpinDamage, OwnerCharacter.Get());
        OnMantornSpinHit(Hit, SpinDamage);
    }

    OnMeleeSwing.Broadcast(true);
    OnMantornSpinStart();

    if (GEngine && WTBRShouldLogValidation())
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
            FString::Printf(TEXT("[Mantorn] Spin AOE — %d hit(s)"), Hits.Num()));
    }

    if (GetWorld())
    {
        bIsOnCooldown = true;
        GetWorld()->GetTimerManager().SetTimer(
            CooldownTimer, this, &UWTBRMantornTrigger::OnCooldownExpired,
            M.ActionCooldown, false);
    }
    return true;
}

void UWTBRMantornTrigger::PerformSpinSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid() || !GetWorld()) return;
    if (!IsValid(DataAsset)) return;

    const float Radius = DataAsset->MeleeHitbox.MantornSpinRadius;
    const FVector Center = OwnerCharacter->GetActorLocation();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerCharacter.Get());
    Params.bTraceComplex = false;

    GetWorld()->SweepMultiByChannel(
        OutHits, Center, Center, FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(Radius),
        Params);

    UE_LOG(LogTemp, Log,
        TEXT("WTBR Mantorn SpinSweep radius %.0f: %d hit(s)"),
        Radius, OutHits.Num());

#if ENABLE_DRAW_DEBUG
    DrawDebugSphere(GetWorld(), Center, Radius, 16,
        OutHits.Num() > 0 ? FColor::Red : FColor::Green,
        false, 0.5f);
#endif
}

void UWTBRMantornTrigger::PerformWhipSweep(TArray<FHitResult>& OutHits)
{
    if (!OwnerCharacter.IsValid() || !GetWorld()) return;
    if (!IsValid(DataAsset)) return;

    const FWTBRMantornParams& M = DataAsset->MantornParams;

    // Aim along control rotation so the whip goes where the player looks
    // (bOrientRotationToMovement means actor forward != camera forward).
    FVector Forward = OwnerCharacter->GetActorForwardVector();
    FQuat   SweepRot = OwnerCharacter->GetActorQuat();
    if (AController* C = OwnerCharacter->GetController())
    {
        const FRotator CtrlRot = C->GetControlRotation();
        Forward  = FRotationMatrix(CtrlRot).GetUnitAxis(EAxis::X);
        SweepRot = FQuat(FRotator(0.f, CtrlRot.Yaw, 0.f));
    }

    const FVector Origin = OwnerCharacter->GetActorLocation();
    const FVector Start  = Origin + Forward * DataAsset->MeleeHitbox.ForwardOffset;
    const FVector End    = Origin + Forward * M.WhipReach;

    // Anti-tunneling: sweep the capsule along the whole whip path, not just a point.
    SweepCapsuleFromTo(Start, End, SweepRot, M.WhipRadius,
        DataAsset->MeleeHitbox.CapsuleHalfHeight, OutHits);

    UE_LOG(LogTemp, Log,
        TEXT("WTBR Mantorn WhipSweep reach %.0f: %d hit(s)"),
        M.WhipReach, OutHits.Num());

#if ENABLE_DRAW_DEBUG
    DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f,
        DataAsset->MeleeHitbox.CapsuleHalfHeight, M.WhipRadius, SweepRot,
        OutHits.Num() > 0 ? FColor::Red : FColor::Green, false, 0.5f);
#endif
}
