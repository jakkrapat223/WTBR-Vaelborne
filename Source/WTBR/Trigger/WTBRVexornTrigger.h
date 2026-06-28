// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBRVexornTrigger.generated.h"

class AWTBRCharacter;

// Vexorn — Signal Block (Passive Sub): while equipped as Sub-Trigger, pulses a sphere
// overlap every 0.5 s and suppresses radar pings for all enemies inside VexornSuppressionRadius.
// Characters that leave the radius are automatically unsuppressed. No button press required.
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

    // VFX hook — called manually on server when bSuppressionActive changes.
    // NOTE: UObject cannot use UE's DOREPLIFETIME system; if client aura VFX is needed,
    // wire this via the owning AWTBRCharacter's replicated state in a later phase.
    UFUNCTION(BlueprintImplementableEvent, Category = "Vexorn | VFX")
    void OnRep_bSuppressionActive();

protected:
    // True while the suppression pulse timer is running.
    bool bSuppressionActive = false;

private:
    // Repeating pulse — fires TickSuppression every 0.5 s (NOT Tick/TickComponent).
    FTimerHandle TimerHandle_SuppressionPulse;

    // Characters currently inside the suppression radius.
    TArray<TWeakObjectPtr<AWTBRCharacter>> TrackedSuppressed;

    // Logging throttle only; does not affect gameplay state.
    float LastPulseLogTime = -9999.0f;
    int32 LastRawOverlapCount = INDEX_NONE;
    int32 LastTargetCount = INDEX_NONE;

    // Sphere overlap → array diff → RegisterSignalBlock / UnregisterSignalBlock.
    void TickSuppression();

    // UnregisterSignalBlock for every tracked character, then empties TrackedSuppressed.
    void RemoveSuppressionFromAll();
};
