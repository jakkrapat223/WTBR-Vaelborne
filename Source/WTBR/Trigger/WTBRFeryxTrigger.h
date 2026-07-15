// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRFeryxTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFeryxTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    // Used by the Feryx+Feryx Mantorn gesture path when a press resolves as a
    // normal tap rather than a completed two-button transform.
    void ResolveTap(bool bIsDualWield);

    virtual bool Activate_Implementation(const FInputActionValue& InputValue, bool bIsDualWield) override;
    virtual void OnTriggerActivated_Implementation(AActor* OwnerActor, bool bIsMain) override;
    virtual void OnTriggerDeactivated_Implementation(AActor* OwnerActor, bool bIsMain) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Feryx | VFX")
    void OnFeryxSwing(bool bIsDualWield, bool bDidHit);

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Feryx | VFX")
    void OnFeryxHit(const FHitResult& Hit, float DamageDealt);

protected:
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override;
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override;

private:
    void ThrowBladeStars();
    void ClearStarCooldown();
    float HoldStartTime = 0.0f;
    bool bHoldPending = false;
    bool bPendingDualWield = false;
    bool bStarOnCooldown = false;
    FTimerHandle StarCooldownTimer;
};
