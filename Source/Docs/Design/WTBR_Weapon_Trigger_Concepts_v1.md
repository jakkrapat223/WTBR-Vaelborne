# WTBR Weapon Trigger Concepts v1

Project: WTBR / Vaelborne: Dominion
Status: Concept direction locked for base weapons v1
Scope: Design documentation only

## Naming / Implementation Notes

This document describes target weapon design direction, not guaranteed current implementation parity.

Some design names may differ from current C++ class names, DataAssets, or legacy internal trigger names.

Before implementation, map each design weapon to its current code asset/class.

| Design Name | Current Code / Existing Reference | Note |
|---|---|---|
| Lacern | DA_Lacern / UWTBRLacernTrigger | Active/current melee blade direction |
| Feryx | DA_Feryx / UWTBRFeryxTrigger | Existing inactive melee trigger; design direction updated |
| Mantis / Mantorn | UWTBRMantornTrigger appears to exist as an independent base weapon | Needs decision; do not assume Feryx+Feryx is already supported |
| Astria / Asteroid | TBD, likely Solux/Solgrix-related depending on current implementation | Design name; mapping required before coding |
| Hound | TBD | Design direction only unless matching class already exists |
| Viper | TBD | Design direction only unless matching class already exists |
| Meteo | TBD, likely Fulgrix/Meteor-related depending on current implementation | Design name; mapping required before coding |

## Weapon Taxonomy

Base Trigger Weapons:
1. Lacern
2. Astria / Asteroid
3. Hound
4. Viper
5. Meteo
6. Feryx
7. Mantis / Mantorn

Mantis / Mantorn:
- Current implementation appears to have Mantorn/Mantis as an independent base weapon.
- Design target: segmented Trion blade whip.
- Feryx + Feryx composite remains a future design option only and is not current implementation.

Future Composite Option:
- Feryx + Feryx composite Mantis is a future option only and requires a separate melee composite system decision.

## 1. Lacern

Role:
- Standard melee blade
- Reach, timing, slash control
- Already considered final direction

Tap:
- Normal blade slash

Hold:
- Senku-style extended slash / range extension

Identity:
- Safer reach than Feryx
- Less burst than Feryx
- More standard melee control

## 2. Astria / Asteroid

Role:
- Straight projectile
- Reliable damage
- Basic cube bullet family baseline

Tap:
- Splits into 5 cubes
- Fires horizontally / parallel
- Direct projectile behavior

Hold:
- Costs additional mana
- Adds 1–2 extra cubes
- Slightly increases projectile speed

Initial tuning direction:
- Damage per cube: 15–20
- Values should be DA-driven

## 3. Hound

Role:
- Homing pressure

Tap:
- Splits into 6 cubes
- Fires horizontally / parallel
- Each cube can home toward the first valid target that enters range

Homing rules:
- Homing locks only once
- No reacquire after block, miss, or collision
- Homing duration around 4–5 seconds
- Homing should not run forever

Hold:
- Uses preset timing behavior
- Example: first 2 cubes launch first, cubes 3–6 launch on a second timing wave
- Charge helps determine range/timing behavior

Initial tuning direction:
- Damage per cube: 10–15
- Homing behavior should be DA-driven where possible

## 4. Viper

Role:
- Path trick / curve projectile

Tap:
- Splits into 6 cubes
- Fires horizontally / parallel, similar to Astria baseline

Hold:
- Uses preset path behavior
- Player can configure preset path logic
- Limited to 3–4 turning points

Example preset:
- Cubes 1–3 curve right, then turn back toward center
- Cubes 4–6 curve left, then turn back toward center

Design constraint:
- Use preset waypoints/turn points first
- Do not implement freeform path drawing for MVP

Initial tuning direction:
- Damage per cube: 10–15
- Turn count and path preset should be DA-driven

## 5. Meteo

Role:
- Explosive / trap / zoning

Tap:
- Splits into 4 cubes
- Fires horizontally / parallel
- Higher damage per cube than Astria/Hound/Viper

Hold:
- If no active Meteo Trap exists: place 1 Meteo Trap
- If active Meteo Trap already exists: Hold detonates the existing trap
- Player must use Tap if they want to fire normally while a trap is active

Trap rules:
- Only 1 active Meteo Trap per player
- Trap has a lifetime
- If lifetime expires, trap disappears by itself
- Trap should not tick every frame
- Server handles detonation damage
- Client handles VFX

Initial tuning direction:
- Tap damage per cube: 25–30
- Trap explosion damage: 100–150
- Active trap cap per player: 1

## 6. Feryx

Role:
- Close-range burst
- Anti-shield utility

Feryx final direction:
- Form B short Trion blade materialized from hand/wrist.
- Not a full body-grown weapon concept.
- Tap = close-range high-damage short blade slash.
- Range shorter than Lacern.
- Damage higher than Lacern due to higher risk.
- Hold = straight Feryx Star / blade-star throw, similar to Astria-style straight projectile.
- No homing.
- No curve path.
- On Defender shield hit: reduce shield HP normally and apply Brittle to Defender only.
- Brittle duration around 2 seconds.
- Brittle increases shield damage taken by around 15–20%.

## Mantis / Mantorn Decision Required

Current code appears to contain UWTBRMantornTrigger as an independent base weapon with its own behavior.

Design target discussed:
- Mantis / Mantorn should use a segmented Trion blade whip visual style.
- Tap = forward whip slash.
- Hold = short-range AOE slash around the player.

Important:
- Do not treat Feryx + Feryx composite as current implementation.
- Feryx + Feryx composite is a future design option only.
- Implementing Mantis as Feryx + Feryx would require new melee composite support.
- Before coding, decide one of these routes:
  A. Keep Mantorn as an independent base weapon and align it to the whip/AOE design.
  B. Create a future Feryx + Feryx composite system.
  C. Rename one concept to avoid conflict.

Recommended current documentation stance:
- Treat Mantorn/Mantis as an independent base weapon for now.
- Keep Feryx + Feryx composite as a future design note only.

## Design Rule

All tunable values should be DataAsset-driven where possible:
- cube count
- projectile count
- damage
- speed
- lifetime
- mana cost
- cooldown
- hold charge scaling
- homing duration
- Viper turn count
- Meteo trap lifetime
- Meteo explosion damage/radius
- Feryx Brittle duration
- Feryx Brittle shield damage multiplier
