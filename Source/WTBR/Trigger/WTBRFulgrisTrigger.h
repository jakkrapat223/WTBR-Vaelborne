// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRFulgrisTrigger.generated.h"

// Fulgris — Lightning (Fastest Sniper, lower damage)
// NOTE: Fulgris (Sniper) is distinct from Fulgrix (explosive Gunner)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFulgrisTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Fulgris | VFX")
    void OnFulgrisFired();
};
