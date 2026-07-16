// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRCoilvynCompositeBehavior.generated.h"

/** Registry-selected curved-path projectile that transitions to cast-time target homing. */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRCoilvynCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
