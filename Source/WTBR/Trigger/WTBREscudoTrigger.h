// Copyright Vaelborne: Dominion Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRDefenseTrigger.h"
#include "WTBREscudoTrigger.generated.h"

class AWTBRAegornWallActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEscudoWallPlaced);

// Escudo — fixed-position Trion wall (canon: erected by touching a surface,
// opaque, cannot be moved/reshaped, far more durable than Shield/Aegorn and
// can stop attacks Shield can't, at a high Vael cost). Split out from Aegorn
// because canon treats these as two distinct Triggers, not tap/hold of one.
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBREscudoTrigger : public UWTBRDefenseTrigger
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category = "WTBR | Escudo | Events")
    FOnEscudoWallPlaced OnWallPlaced;

    virtual bool Activate_Implementation(
        const FInputActionValue& InputValue,
        bool bIsDualWield) override;

    UFUNCTION(BlueprintImplementableEvent, Category = "WTBR | Escudo | VFX")
    void OnEscudoWallSpawned(AWTBRAegornWallActor* WallActor);

private:
    UPROPERTY(EditDefaultsOnly, Category = "WTBR | Escudo | Wall")
    TSoftClassPtr<AWTBRAegornWallActor> WallActorClass;
};
