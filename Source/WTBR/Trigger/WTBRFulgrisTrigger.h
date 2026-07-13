// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRSniperTrigger.h"
#include "WTBRFulgrisTrigger.generated.h"

// Fulgris — Lightning (Fastest Sniper, lower damage, bCanPenetrate=false)
// NOTE: Fulgris (Sniper) is distinct from Fulgrix (explosive Gunner)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRFulgrisTrigger : public UWTBRSniperTrigger
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    virtual float GetCooldownDuration() const override;
    virtual float GetZoomFOV() const override;
};
