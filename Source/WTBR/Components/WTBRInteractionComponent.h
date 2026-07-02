// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WTBRInteractionComponent.generated.h"

class AWTBRCorpseLootContainerActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWTBRCorpseLootInteractRequested, AWTBRCorpseLootContainerActor*, Container);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRInteractionComponent();

    UFUNCTION(BlueprintCallable, Category="WTBR|Interaction")
    AWTBRCorpseLootContainerActor* GetFocusedCorpseLootContainer() const;

    UFUNCTION(BlueprintCallable, Category="WTBR|Interaction")
    FText GetFocusedInteractionPromptText() const;

    // Fired on the owning client when the player requests to open a focused corpse
    // loot container. Bind in Blueprint HUD/UI to open the loot widget.
    // Broadcasts only when a valid container passes CanBeInteractedWithBy.
    // Does not mutate any gameplay state; server RPC is called only on pickup confirm.
    UPROPERTY(BlueprintAssignable, Category="WTBR | Interaction")
    FWTBRCorpseLootInteractRequested OnCorpseLootInteractRequested;

    // Called by AWTBRCharacter::Interact() on the owning client.
    // Returns true if OnCorpseLootInteractRequested was broadcast, false otherwise.
    UFUNCTION(BlueprintCallable, Category="WTBR | Interaction")
    bool RequestCorpseLootInteract();

private:
    UPROPERTY(EditDefaultsOnly, Category="WTBR|Interaction")
    float InteractionTraceDistance = 300.0f;
};
