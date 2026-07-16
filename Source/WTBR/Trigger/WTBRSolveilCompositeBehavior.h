// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRSolveilCompositeBehavior.generated.h"

/** Registry-selected penetrating projectile behavior following an authored curved path. */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSolveilCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
