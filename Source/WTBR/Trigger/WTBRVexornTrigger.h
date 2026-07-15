// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBRVexornTrigger.generated.h"

class AWTBRCharacter;

// Vexorn — Bagworm-style radar cloak. While equipped as the active Sub-Trigger,
// the owner is omitted from radar and pays a continuous Vael drain.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVexornTrigger : public UWTBRDefenseTrigger
{
    GENERATED_BODY()

public:
    // Passive trigger — button press is a no-op.
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void OnReleased_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void Deactivate_Implementation() override;

    // Starts suppression pulse when Vexorn becomes the active Sub-Trigger.
    virtual void OnEquipped() override;

    // Stops suppression pulse, removes all active blocks when Vexorn is swapped out.
    virtual void OnUnequipped() override;

    // VFX hook — called when the cloak starts or ends.
    UFUNCTION(BlueprintImplementableEvent, Category = "Vexorn | VFX")
    void OnRep_bSuppressionActive();

protected:
    // True while the owner is currently cloaked on radar.
    bool bSuppressionActive = false;

private:
    FTimerHandle TimerHandle_CloakDrain;
    void TickCloakDrain();
    void SetCloakActive(bool bNewActive);
};
