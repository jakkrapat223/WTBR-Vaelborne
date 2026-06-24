// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRNyxveilTrigger.generated.h"

// Nyxveil — Yomi (Scan): repeating sphere scan that pings all detected pawns on the radar
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRNyxveilTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual void Deactivate_Implementation() override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nyxveil | VFX")
    void OnNyxveilActivated();

    // Fired after each scan with the number of actors detected
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Nyxveil | VFX")
    void OnNyxveilDetected(int32 EnemyCount);

private:
    FTimerHandle PingTimerHandle;
    FTimerHandle StopTimerHandle;
    float CachedScanRadius = 0.0f;

    void DoScan();
    void StopScan();
};
