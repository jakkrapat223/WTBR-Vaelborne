// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRSolgrixCompositeBehavior.generated.h"

/** Registry-selected straight shaped-charge projectile behavior. */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRSolgrixCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
