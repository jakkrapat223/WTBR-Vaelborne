// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRArcvenTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRArcvenTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // Fires this Arc Wave as a Lacern-hold charge tier (canon Senkū Trigger
    // Option), bypassing this trigger's own cooldown/Vael/Activate flow — the
    // caller (Lacern's hold-input routing) already validated and charged Vael
    // against ITS OWN DataAsset (this Arcven's VaelCostPerUse). Damage/Range
    // are the caller's charge-tier values (weak or full), not read from
    // ArcvenParams directly, so the same executor works for either tier.
    void FireChargedWave(float Damage, float Range, bool bIsDualWield);

    // Server-only VFX hook (projectile replication handles client visuals).
    // For client muzzle flash: bind OnMeleeSwing in Blueprint.
    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Arcven | VFX")
    void OnArcvenFire(bool bIsDual, const FVector& FireDirection);

protected:
    // Arcven fires projectiles — no capsule sweep needed
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override {}
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override {}

private:
    void FireArcWave(const FVector& Direction, float Damage, float Range);
};
