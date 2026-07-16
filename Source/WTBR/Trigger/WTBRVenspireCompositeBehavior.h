// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRVenspireCompositeBehavior.generated.h"

/** Registry-selected simultaneous multi-target homing behavior. */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRVenspireCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
