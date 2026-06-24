// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRTriggerBase.h"
#include "WTBRPiercexTrigger.generated.h"

// Piercex — Ibis (Penetrating Sniper, highest damage, bCanPenetrate=true)
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRPiercexTrigger : public UWTBRTriggerBase
{
    GENERATED_BODY()

public:
    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Piercex | VFX")
    void OnPiercexFired();

    // Fired each time the bullet passes through a character while bCanPenetrate is active
    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Piercex | VFX")
    void OnPiercexPenetrate(AActor* PenetratedActor);
};
