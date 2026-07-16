// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRHomingExplosiveCompositeBehavior.generated.h"

/** Registry-selected straight missile behavior with optional target homing and explosion. */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRHomingExplosiveCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
