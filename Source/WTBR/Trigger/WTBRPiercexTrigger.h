// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRSniperTrigger.h"
#include "WTBRPiercexTrigger.generated.h"

// Piercex — Ibis (Penetrating Sniper, highest damage, bCanPenetrate=true)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRPiercexTrigger : public UWTBRSniperTrigger
{
    GENERATED_BODY()

public:
    virtual float GetCooldownDuration() const override;
    virtual float GetZoomFOV() const override;

protected:
    virtual bool ExecuteFire() override;
};
