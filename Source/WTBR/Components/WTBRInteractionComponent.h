// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WTBRInteractionComponent.generated.h"

class AWTBRCorpseLootContainerActor;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WTBR_API UWTBRInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWTBRInteractionComponent();

    UFUNCTION(BlueprintCallable, Category="WTBR|Interaction")
    AWTBRCorpseLootContainerActor* GetFocusedCorpseLootContainer() const;

private:
    UPROPERTY(EditDefaultsOnly, Category="WTBR|Interaction")
    float InteractionTraceDistance = 300.0f;
};
