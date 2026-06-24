// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRLacernTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRLacernTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void Deactivate_Implementation() override;

protected:
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override;
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override;

private:
    float CurrentExtendDist = 0.0f;
    bool  bIsExtending      = false;
    bool  bIsDualWieldSwing = false;

    // Per-swing hit deduplication — prevents the same Actor taking
    // damage more than once during a single extend/retract cycle
    TSet<AActor*> HitActorsThisSwing;

    FTimerHandle ExtendTimer;
    FTimerHandle RetractTimer;

    // Previous blade tip position — used by SweepCapsuleFromTo
    // to prevent tunneling when blade moves fast between ticks
    FVector PreviousBladePos = FVector::ZeroVector;

    void StartExtend(bool bDualWield);
    void TickExtend();
    void StartRetract();
    void TickRetract();

    UFUNCTION()
    void OnRetractComplete();

    void SweepAtCurrentPosition(float RightOffset, TArray<FHitResult>& OutNewHits);

    static constexpr float EXTEND_TICK  = 0.016f;
    static constexpr float RETRACT_TICK = 0.016f;
};
