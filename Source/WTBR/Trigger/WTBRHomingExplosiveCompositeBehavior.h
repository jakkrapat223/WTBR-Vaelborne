// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRHomingExplosiveCompositeBehavior.generated.h"

/**
 * Registry-selected straight missile behavior with optional target homing and explosion.
 *
 * Shared by Fulgvyn and Solhunt. With a Hound preset armed it fires a swept volley
 * instead — see FireSweptVolley — but that route only engages when the resolved
 * preset actually asks for it, so every other caller keeps the single-missile shot.
 */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRHomingExplosiveCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
