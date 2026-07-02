# WTBR Locked Design Decisions

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: 34bf3ec Add positive corpse loot lifetime automation test  
Automation confirmed: WTBR.CorpseLoot PASS 11/11

`WTBR.CorpseLoot.ContainerLifetimeCVarPositiveDestroysActor` is committed and pushed at 34bf3ec. Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` is local-only and must never be staged or committed.

Status tags:

- `LOCKED_MECHANIC`: Decided mechanic or rule.
- `TUNABLE_STARTER`: Starter value expected to be tuned.
- `PROPOSED_DETAIL`: Proposed design detail needing final approval.
- `UNKNOWN`: Placeholder, unresolved, or final spec missing.

## Core Loop

- `LOCKED_MECHANIC`: BR `MaxLives = 2` / one respawn.
- `LOCKED_MECHANIC`: RankWar `MaxLives = 1` / no respawn after Eliminated.
- `LOCKED_MECHANIC`: Full HP -> HP depleted -> Downed -> bleed-out timer or DownedHP depleted -> Eliminated.
- `TUNABLE_STARTER`: `DownedBleedOutSeconds` starts at 30s, tunable via `MatchModeRules` / DataAsset.
- `TUNABLE_STARTER`: `ReviveDurationSeconds` starts at 5-8s, recommended 6s, tunable.
- `LOCKED_MECHANIC`: Corpse loot drops on Eliminated only, not Downed.
- `LOCKED_MECHANIC`: Respawn grants only base preset; looted items stay in corpse box.
- `PROPOSED_DETAIL` / `UNKNOWN`: Respawn point rules remain server-approved selectable points and need final spec.

## Corpse Loot

- `LOCKED_MECHANIC`: Container box is the production target after B7.
- `LOCKED_MECHANIC`: Keep legacy default until B7 passes.
- `LOCKED_MECHANIC`: CVar `WTBR.UseCorpseLootContainerOnDeath` supports `-1` rules / `0` legacy / `1` container.
- `LOCKED_MECHANIC`: Corpse box persists entire active match; cleanup happens on match end, reset, or travel.
- `TUNABLE_STARTER`: Loot interaction range starts at 200cm.
- `LOCKED_MECHANIC`: Bag-style corpse UI.
- `LOCKED_MECHANIC`: Loot filter none; Black Triggers are lootable.
- `LOCKED_MECHANIC`: Container state is realtime replicated.

## Corrected Slot Swap

- `LOCKED_MECHANIC`: Corpse container slot swap is an atomic same-container swap.
- `LOCKED_MECHANIC`: Selected corpse item moves into player target slot.
- `LOCKED_MECHANIC`: Old trigger from target slot moves back into the same corpse container.
- `LOCKED_MECHANIC`: Do not spawn old trigger as `AWTBRDroppedTriggerActor` for corpse-container swap.
- `LOCKED_MECHANIC`: Legacy dropped-trigger path remains for A-mode fallback, ground loot, rollback/legacy behavior, and non-container cases only.

## Energy Projectile Cube Split

- `LOCKED_MECHANIC`: Solux = Asteroid style straight cube split.
- `LOCKED_MECHANIC`: Venyx = Hound style homing cube split.
- `LOCKED_MECHANIC`: Fulgrix = Meteor style AoE cube split.
- `LOCKED_MECHANIC`: Serpveil = Viper style single path projectile, not cube split.
- `LOCKED_MECHANIC`: Snipers Telorn, Piercex, and Fulgris do not cube split.
- `LOCKED_MECHANIC`: Damage budget is divided across cubes.
- `UNKNOWN`: Rounding rule.

## Serpveil

- `LOCKED_MECHANIC`: Charge path with proportional Vael cost.
- `LOCKED_MECHANIC`: Shapes are Curve, S-Curve, Hook, Spiral, and Boomerang.
- `PROPOSED_DETAIL`: Custom player-drawn paths are future S2/S3 work, not prototype scope.

## Composite Bullets Canonical Mapping

- `LOCKED_MECHANIC`: Solgrix = Solux + Fulgrix -> Straight explosive shot.
- `LOCKED_MECHANIC`: Dualux = Solux + Solux -> Twin straight shot.
- `LOCKED_MECHANIC`: Solveil = Solux + Serpveil -> Curving/charged straight shot.
- `LOCKED_MECHANIC`: Solhunt = Solux + Venyx -> Fast homing shot.
- `LOCKED_MECHANIC`: Venspire = Venyx + Venyx -> Advanced multi-homing.
- `LOCKED_MECHANIC`: Fulgvyn = Fulgrix + Venyx -> Homing explosive missile.
- `LOCKED_MECHANIC`: Coilvyn = Serpveil + Venyx -> Curving homing snake shot.
- `LOCKED_MECHANIC`: Catarix = Fulgrix + Fulgrix -> Heavy cluster explosion.
- `LOCKED_MECHANIC`: Labyrn = Serpveil + Serpveil -> Labyrinth trajectory/zoning.
- `LOCKED_MECHANIC`: Ignivex = Solux + Fulgrix variant -> Piercing/ignition explosive line.
- `LOCKED_MECHANIC`: Acervyn naming lock: normal Burst Homing Trigger; Venyx + Venyx is Venspire, not Acervyn.

## BR Match

- `LOCKED_MECHANIC`: Win condition is last team standing.
- `LOCKED_MECHANIC`: Ranking by alive players.
- `TUNABLE_STARTER`: Prototype player count 15, production player count 60.
- `LOCKED_MECHANIC`: Shrinking zone uses phase-based flat HP drain plus healing reduction.

## Economy

- `LOCKED_MECHANIC`: BR has passive Vael regen.
- `LOCKED_MECHANIC`: RankWar has no passive regen; kill/down/assist rewards apply.
- `UNKNOWN`: Final costs, rewards, and max pool are placeholders.

## Black Trigger

- `LOCKED_MECHANIC`: Capture-zone acquisition.
- `LOCKED_MECHANIC`: Enemy in zone pauses gauge.
- `LOCKED_MECHANIC`: Leaving zone resets gauge.
- `LOCKED_MECHANIC`: Dedicated Black Trigger slot.
- `LOCKED_MECHANIC`: Equipped Black Trigger disables Normal Triggers.
- `LOCKED_MECHANIC`: Unequip restores Normal Trigger usage.
- `LOCKED_MECHANIC`: Drops into corpse box on Eliminated.
- `LOCKED_MECHANIC`: Corpse box must indicate Black Trigger.
- `TUNABLE_STARTER`: One event per match, 10-15 min or zone phase 3, gauge 45-60s.
- `UNKNOWN`: Pool, balance, and final indicator.

## Quick Slot

- `LOCKED_MECHANIC`: Hold Q = Main wheel.
- `LOCKED_MECHANIC`: Hold E = Sub wheel.
- `LOCKED_MECHANIC`: Player can move normally while wheel open.
- `LOCKED_MECHANIC`: Mouse direction selects trigger.
- `LOCKED_MECHANIC`: Release confirms.
- `PROPOSED_DETAIL`: Active-trigger change implementation is S2 if it creates a new input-to-server path.

## Infrastructure

- `LOCKED_MECHANIC`: Prototype listen server plus manual join.
- `PROPOSED_DETAIL`: Dedicated server later.
- `PROPOSED_DETAIL`: Matchmaking and persistence post-prototype.

