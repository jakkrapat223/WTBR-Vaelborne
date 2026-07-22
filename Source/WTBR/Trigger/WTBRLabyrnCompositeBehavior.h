// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Trigger/WTBRCompositeBehaviorBase.h"

#include "WTBRLabyrnCompositeBehavior.generated.h"

/**
 * Labyrn (Viper + Viper) — reaches further, turns more, and gets there faster.
 *
 * The lanes are ordinary Viper lanes: authored by the player, curved, no homing, no
 * explosion, damage budget split across the cubes like every other composite. Three
 * things separate it from the Viper it is built out of, and only one of them is
 * code here:
 *   - RANGE, authored on the definition (PathRange).
 *   - TURNS, eight instead of four — see WTBR_TURNS_PER_VIPER. Enforced in
 *     ResolveCompositeCubePaths, which reads the budget off the definition's own
 *     archetypes, so this class does nothing special to earn it.
 *   - SPEED, which is this class: every cube flies faster than a Viper cube for the
 *     first four fifths of its path and then eases off over the last one.
 *
 * An earlier version made every lane land on the same frame by giving each cube its
 * own speed. That was cut deliberately. It could only synchronise by SLOWING every
 * lane down to match the longest one, which made Labyrn slower than a plain Viper on
 * every lane but one — the exact opposite of what it is for. It also fought the way
 * the presets are meant to be used: lanes are authored at different lengths on
 * purpose, some reaching long and some cutting in close, and forcing them to agree
 * on an arrival time destroys the close ones' whole reason for existing. Lanes now
 * simply arrive when their own length says they do. A player who wants a
 * simultaneous arrival authors lanes of equal length and gets one.
 */
UCLASS(BlueprintType, Blueprintable)
class WTBR_API UWTBRLabyrnCompositeBehavior : public UWTBRCompositeBehaviorBase
{
    GENERATED_BODY()

public:
    virtual bool ExecuteComposite(
        AWTBRCharacter* OwningCharacter,
        const FWTBRCompositeDefinition& Definition) override;
};
