// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "Trigger/WTBRTriggerDataAsset.h" // EWTBRSerpveilShape
#include "WTBRSerpveilTrigger.generated.h"

// Serpveil — Viper (Curved Path Gunner, Phase 4 Hybrid Path System)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSerpveilTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual void InitializeTrigger(
        AWTBRCharacter* InOwnerCharacter,
        UWTBRTriggerDataAsset* InDataAsset) override;

    // No-op: S1 schedules its authoritative volley from OnTriggerActivated.
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // Press: validate and commit the server-authoritative S1 windup.
    virtual void OnTriggerActivated_Implementation(
        AActor* OwnerActor, bool bIsMain) override;

    // Release is intentionally a no-op for a committed S1 windup.
    virtual void OnTriggerDeactivated_Implementation(
        AActor* OwnerActor, bool bIsMain) override;

    virtual void Deactivate_Implementation() override;
    virtual void OnUnequipped() override;

    // Deprecated release-fire compatibility entry point. S1 never fires from here.
    void ExecuteServerFire(EWTBRSerpveilShape Shape, FRotator Direction, float ChargedRange);

    bool CancelCharge();
    bool IsCharging() const { return bIsCharging; }

    // Blueprint aim-preview line (client only)
    UFUNCTION(BlueprintCallable, Category = "WTBR | Serpveil")
    TArray<FVector> GetPreviewPathPoints(
        FVector StartLocation, FRotator Direction, float Range) const;

    // ── VFX Hooks ──────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Serpveil | VFX")
    void OnSerpveilChargeStart();

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Serpveil | VFX")
    void OnSerpveilCharging(float ChargePercent);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Serpveil | VFX")
    void OnSerpveilFired(EWTBRSerpveilShape Shape);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Serpveil | VFX")
    void OnSerpveilHit(FVector ImpactPoint);

    // Builds world-space waypoints — public so FireComposite (Labyrn) can reuse it.
    // bMirrorY flips the Y axis of every local waypoint before world-space transform,
    // producing the mirror-image path needed for Labyrn's Core B.
    static TArray<FVector> BuildPathPoints(
        EWTBRSerpveilShape Shape, FVector StartLocation, FRotator Direction, float Range,
        bool bMirrorY = false);

private:
    float ChargeStartTime = 0.0f;
    float LastSerpveilFireTime = -1000000.0f;
    bool  bCachedIsMain   = false;
    bool  bIsCharging     = false;
    FTimerHandle ChargeUpdateTimer;
    FTimerHandle WindupTimer;

    void UpdateChargeProgress();
    void StopChargeTracking();
    void ExecuteSplitVolley();

    UFUNCTION()
    void HandleOwnerDeath();
};
