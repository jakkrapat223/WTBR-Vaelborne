// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/WTBRHUDViewTypes.h"
#include "WTBRInputBindingDisplayLibrary.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class WTBR_API UWTBRInputBindingDisplayLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="WTBR | HUD | Input")
    static FWTBRHUDBindingDisplay ResolveInputActionDisplayName(
        const UInputMappingContext* MappingContext,
        const UInputAction* InputAction);
};
