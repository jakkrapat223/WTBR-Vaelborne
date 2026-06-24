// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBRVexornTrigger.generated.h"

// Vexorn — Signal Block (Passive Defense, Sub Slot Only)
// Hides the owner from enemy Radar / ActionPing while this trigger is the active Sub-Trigger.
// No button press required — effect applies on equip, removed on unequip.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVexornTrigger : public UWTBRDefenseTrigger
{
    GENERATED_BODY()

public:
    // Passive — button press is a no-op
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    // Called by TriggerSetComponent when Vexorn becomes the active Sub-Trigger
    virtual void OnEquipped() override;

    // Called by TriggerSetComponent when Vexorn is replaced as the active Sub-Trigger
    virtual void OnUnequipped() override;

    // VFX hook — Blueprint plays cyber-cloak effect on equip
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Vexorn | VFX")
    void OnVexornActivated();
};
