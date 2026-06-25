// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRBlackTrigger.h"
#include "WTBRNyxveilTrigger.generated.h"

// Nyxveil — Yomi (Scan): repeating sphere scan that pings all detected pawns on the radar
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRNyxveilTrigger : public UWTBRBlackTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;

    // Stop timers if trigger is force-deactivated (death, stun, loadout swap)
    virtual void Deactivate_Implementation() override;

private:
    FTimerHandle ScanTimer;
    FTimerHandle ScanDurationTimer;

    void PerformScan();
    void OnScanExpired();
};
