// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Trigger/WTBRPathPresetTypes.h"

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
    //
    // OutCubeLaunches, when supplied, comes back the same length as OutCubePaths and
    // carries each cube's launch delay and homing radius. A TAP fills it with zeros:
    // tap is the instant, reliable answer, so waves and sweeps belong to hold only.
    static void ResolveCompositeCubePaths(
        const AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition,
        const FVector& SpawnLocation,
        const FRotator& SpawnRotation,
        TArray<TArray<FVector>>& OutCubePaths,
        TArray<FWTBRResolvedCubeLaunch>* OutCubeLaunches = nullptr);

    // Hound route, shared by every Venyx composite: one cube per authored lane,
    // launched in waves, each hunting on proximity rather than at a target chosen
    // before the shot left. Explodes only if the definition says the composite does,
    // so Fulgvyn still bursts and Solhunt still does not.
    static bool FireSweptVolley(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition,
        const FRotator& SpawnRotation,
        const TArray<TArray<FVector>>& CubeWorldPaths,
        const TArray<FWTBRResolvedCubeLaunch>& CubeLaunches);
};
