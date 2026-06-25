// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Trigger/WTBRMeleeTrigger.h"
#include "WTBRMantornTrigger.generated.h"

UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRMantornTrigger : public UWTBRMeleeTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // Called on ALL instances (server + client) via OnMeleeSwing delegate.
    // Implement VFX/SFX in BP_WTBRMantornTrigger.
    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Mantorn | VFX")
    void OnMantornSpinStart();

    // Called on SERVER only — carries damage data.
    // Clients react via OnRep_HP on the hit character.
    UFUNCTION(BlueprintImplementableEvent, Category="WTBR | Mantorn | VFX")
    void OnMantornSpinHit(const FHitResult& Hit, float DamageDealt);

protected:
    virtual void PerformSingleSweep(TArray<FHitResult>& OutHits) override;
    virtual void PerformDualSweep(TArray<FHitResult>& OutHits) override;

private:
    void PerformSpinSweep(TArray<FHitResult>& OutHits);
};
