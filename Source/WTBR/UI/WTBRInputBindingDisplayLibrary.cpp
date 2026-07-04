// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "UI/WTBRInputBindingDisplayLibrary.h"

#include "InputAction.h"
#include "InputMappingContext.h"

FWTBRHUDBindingDisplay UWTBRInputBindingDisplayLibrary::ResolveInputActionDisplayName(
    const UInputMappingContext* MappingContext,
    const UInputAction* InputAction)
{
    FWTBRHUDBindingDisplay Result;

    if (!IsValid(MappingContext) || !IsValid(InputAction))
    {
        return Result;
    }

    for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
    {
        if (Mapping.Action != InputAction || !Mapping.Key.IsValid())
        {
            continue;
        }

        const FText DisplayName = Mapping.Key.GetDisplayName(false);
        if (DisplayName.IsEmpty())
        {
            continue;
        }

        Result.bIsBound = true;
        Result.DisplayName = DisplayName;
        Result.GlyphToken = NAME_None;
        return Result;
    }

    return Result;
}
