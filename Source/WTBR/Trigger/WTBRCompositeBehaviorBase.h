// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "WTBRCompositeBehaviorBase.generated.h"

class AWTBRCharacter;
struct FWTBRCompositeDefinition;

/** Abstract behavior hook selected by a composite registry definition. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class WTBR_API UWTBRCompositeBehaviorBase : public UObject
{
    GENERATED_BODY()

public:
    // Concrete composite behaviors implement their fire logic in later build steps.
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition)
        PURE_VIRTUAL(UWTBRCompositeBehaviorBase::ExecuteComposite, return false;);
};
