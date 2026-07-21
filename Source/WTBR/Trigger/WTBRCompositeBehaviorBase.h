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

protected:
    // One world-space path per cube the composite should fire.
    //
    // For a composite built from a Serpveil this prefers the player's ARMED
    // Serpveil preset over the definition's authored PathPreset, so Viper's
    // trajectory control carries into every composite it is part of. Falls back
    // to the definition whenever the composite has no Serpveil in it, or the
    // player has nothing armed — so an unarmed player always still fires.
    static void ResolveCompositeCubePaths(
        const AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition,
        const FVector& SpawnLocation,
        const FRotator& SpawnRotation,
        TArray<TArray<FVector>>& OutCubePaths);
};
