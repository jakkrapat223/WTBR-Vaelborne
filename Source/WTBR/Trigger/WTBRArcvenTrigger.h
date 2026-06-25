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

    // Server-only VFX hook (projectile replication handles client visuals).
    // For client muzzle flash: bind OnMeleeSwing in Blueprint.
    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Arcven | VFX")
    void OnArcvenFire(bool bIsDual, const FVector& FireDirection);

protected:
    // Arcven fires projectiles — no capsule sweep needed
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override {}
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override {}

private:
    void FireArcWave(const FVector& Direction, float Damage);
};
