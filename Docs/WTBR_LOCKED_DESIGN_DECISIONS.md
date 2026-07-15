# WTBR Locked Design Decisions

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: 6470cd1 Document canonical input binding lock
Automation confirmed: WTBR.CorpseLoot PASS 22/22

Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` is local-only and must never be staged or committed.

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
- `LOCKED_MECHANIC`: Serpveil = Viper style cube-split volley (S1, implemented): tap conjures then fires a straight cube volley after a windup; cube count/formation/per-cube damage are DataAsset-driven.
- `LOCKED_MECHANIC`: Snipers Telorn, Piercex, and Fulgris do not cube split.
- `LOCKED_MECHANIC`: Damage budget is divided across cubes.
- `UNKNOWN`: Rounding rule.

## Serpveil

- `LOCKED_MECHANIC`: S1 (Tap, implemented and verified baseline) = straight cube volley, flat Vael cost per shot (NOT proportional to charge/path).
- `LOCKED_MECHANIC`: S2 (Hold, Armed Preset path system — PENDING, not yet built) = path presets are polylines (`FWTBRPathPreset`, multi-lane `FWTBRPathLane` waypoints), not the 5 named smooth shapes (Curve/S-Curve/Hook/Spiral/Boomerang) previously listed here.
- `PROPOSED_DETAIL`: Custom player-drawn paths are future work beyond S2, not prototype scope.

## Composite Bullets Canonical Mapping

- `LOCKED_MECHANIC`: Solgrix = Solux + Fulgrix -> shaped-charge cone forward (direct-hit kinetic + forward cone explosion; NOT a round AOE — base Fulgrix already does straight+round-explode).
- `LOCKED_MECHANIC`: Dualux = Solux + Solux -> Twin straight shot.
- `LOCKED_MECHANIC`: Solveil = Solux + Serpveil -> Curving/charged straight shot.
- `LOCKED_MECHANIC`: Solhunt = Solux + Venyx -> Fast homing shot.
- `LOCKED_MECHANIC`: Venspire = Venyx + Venyx -> Advanced multi-homing.
- `LOCKED_MECHANIC`: Fulgvyn = Fulgrix + Venyx -> Homing explosive missile.
- `LOCKED_MECHANIC`: Coilvyn = Serpveil + Venyx -> Curving homing snake shot.
- `LOCKED_MECHANIC`: Catarix = Fulgrix + Fulgrix -> Heavy cluster explosion.
- `LOCKED_MECHANIC`: Labyrn = Serpveil + Serpveil -> Labyrinth trajectory/zoning.
- `LOCKED_MECHANIC`: Ignivex = Fulgrix + Serpveil -> Tomahawk (canon), explosive curved path. CHANGED 2026-07-14: previously listed here as a Solux+Fulgrix variant, which duplicated Solgrix's pair — superseded.
- `LOCKED_MECHANIC`: Acervyn naming lock: normal Burst Homing Trigger; Venyx + Venyx is Venspire, not Acervyn.
- `LOCKED_MECHANIC`: Acervyn is a standalone advanced Gunner (`BulletArchetype = NonCombinable`), excluded from the composite pair resolver — it is NOT one of the 4 composite-source archetypes (those are Solux, Fulgrix, Venyx, Serpveil). Holding Acervyn and pressing Merge shows "Composite unavailable" and reserves no Vael.

## BR Match

- `LOCKED_MECHANIC`: Win condition is last team standing.
- `LOCKED_MECHANIC`: Ranking by alive players.
- `TUNABLE_STARTER`: Prototype player count 15, production player count 60.
- `LOCKED_MECHANIC`: Shrinking zone uses phase-based flat HP drain plus healing reduction.

## Economy

- `LOCKED_MECHANIC`: BR has passive Vael regen.
- `LOCKED_MECHANIC`: RankWar has no passive regen; kill/down/assist rewards apply.
- `LOCKED_MECHANIC`: Vael Overcharge temporarily raises MaxVael and grants CurrentVael equal to the bonus; on expiry MaxVael returns to normal and CurrentVael clamps down to normal MaxVael. Vael Overcharge is not passive regen and is BR-only unless match rules explicitly enable it. Respect the existing ban: no passive Vael regen for 3v3 / RankWar / TeamThree15P.
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

## Input Bindings

- `LOCKED_MECHANIC`: All keys are default bindings only and must remain remappable. Do not hard-code physical Q/E/F/LMB/RMB in gameplay logic. Gameplay code must respond to InputActions / semantic actions, not physical keys.
- `LOCKED_MECHANIC`: Q/E are Trigger SELECTION only and must not fire, charge, or execute presets.
- `LOCKED_MECHANIC`: Q hold opens the Main Trigger selection dialog and picks which Trigger slot is active on the Main side; E hold opens the Sub Trigger selection dialog for the Sub side. The dialog UI may be client-local, but the actual active Trigger slot change must be requested by the client and validated/applied by the server.
- `LOCKED_MECHANIC`: Q tap = switch main trigger slot.
- `LOCKED_MECHANIC`: Q hold = open main trigger wheel/dialog (client-local UI).
- `LOCKED_MECHANIC`: E tap = switch sub trigger slot.
- `LOCKED_MECHANIC`: E hold = open sub trigger wheel/dialog (client-local UI).
- `LOCKED_MECHANIC`: F tap = context interact. F is the default key only; never hard-code F in gameplay logic; bind via `IA_Interact` / `InteractAction` so the key can be remapped.
- `LOCKED_MECHANIC`: Interact must NOT be on Q or E. Q/E are reserved for trigger slot tap/hold only.
- `LOCKED_MECHANIC`: Player can move normally while wheel open.
- `LOCKED_MECHANIC`: Mouse direction selects trigger in wheel; release confirms.
- `LOCKED_MECHANIC`: Context interact dispatch — focused corpse/container/chest: open loot UI (client-local, no server RPC for UI open); focused ground item: pick up via server-authoritative validation; focused generic interactable: call that object's interact behavior; no valid target: do nothing.
- `LOCKED_MECHANIC`: Confirming loot pickup or slot swap from a container uses existing server RPC validation (`Server_RequestPickupCorpseLootEntry`).
- `LOCKED_MECHANIC`: Client UI must not mutate inventory, trigger slots, corpse loot entries, or ground item state directly.
- `PROPOSED_DETAIL`: Active-trigger change implementation is S2 if it creates a new input-to-server path.

### Trigger Use (LMB / RMB)

- `LOCKED_MECHANIC`: LMB = Main Trigger use; RMB = Sub Trigger use. Default bindings only, must remain remappable.
- `LOCKED_MECHANIC`: Code currently uses `FireMainAction` / `FireSubAction`. Rename to `UseMainAction` / `UseSubAction` is optional cleanup only and is not required in this docs commit.
- `LOCKED_MECHANIC`: Tap = basic/default immediate use of the active Trigger; tap does not open a preset dialog. Examples: Solux/Serpveil tap = basic fire from TriggerData; Feryx tap = normal melee; Escudo tap = place one wall panel in front.
- `LOCKED_MECHANIC`: Hold = Trigger-defined hold behavior, not always a preset. Examples: Feryx+Senku hold = charge Senku (no preset dialog); Escudo hold = open preset dialog, preview, confirm to commit; Viper/Hound-style projectile hold = open preset dialog then aim/charge/range/pattern commit; Trigger with no hold behavior = no-op or TriggerData-defined fallback.

### Fire-Time Preset

- `LOCKED_MECHANIC`: When a Trigger's hold behavior uses presets, the preset is chosen at fire time via the Trigger-use hold — Main = LMB hold / MainUseAction, Sub = RMB hold / SubUseAction. Presets are never selected through Q/E.
- `PROPOSED_DETAIL`: Phase A / MVP — designer-authored presets in DataAsset only; player selects from allowed presets; no player custom edit/save yet.
- `PROPOSED_DETAIL`: Phase B / deferred — player custom preset editor; SaveGame support; server clamps and validates every editable value.

### Preview Validation

- `LOCKED_MECHANIC`: Client preview is advisory only. For Escudo wall preview, delayed projectile pattern preview, or any preset preview: client may show preview locally; on commit the server re-runs validation, decides success or failure, and spawns the authoritative wall/projectile sequence. No Vael is spent if the server rejects.

## BR Ground Item & Inventory

- `LOCKED_MECHANIC`: BR Ground Item is separate from Dropped Trigger. Dropped Trigger (`AWTBRDroppedTriggerActor`) = Trigger loot only. BR Ground Item = consumables, Vael items, tuning, options, upgrade materials. `AWTBRGroundItemActor` is not created yet.
- `LOCKED_MECHANIC`: F context interact priority — corpse/container/chest -> dropped trigger -> BR ground item -> generic interactable -> no-op. Corpse/container/chest opens loot UI client-local; dropped trigger pickup and BR ground item pickup must go through server RPC.
- `LOCKED_MECHANIC`: MVP consumables = HP and Vael only. Stamina items are excluded for MVP because stamina auto-regenerates and is dodge-only; re-add only if playtest shows exhaustion is too punishing.
- `LOCKED_MECHANIC`: Inventory stacking (Naraka style) — partial stack first -> first empty slot -> reject when full.
- `TUNABLE_STARTER`: Prototype inventory slot count = 14 (design range 12-16); small consumable stack = 4; large consumable stack = 2. All values configurable.
- `LOCKED_MECHANIC`: Tuning swap (PUBG style) — swapped-out tuning returns to inventory; if inventory is full it drops as a BR Ground Item, not a Dropped Trigger.
- `LOCKED_MECHANIC`: Tuning attaches to a specific Trigger slot/instance and must be removable/swappable; if the Trigger drops or enters a corpse cache, its attached tuning follows the Trigger instance.

## Senku (Feryx Option)

- `LOCKED_MECHANIC`: Senku is a Feryx-specific Trigger Option, not tuning; it must be equipped in the same slot as Feryx.
- `LOCKED_MECHANIC`: Dual Senku requires both the active Main and active Sub slot to each contain Feryx + Senku. If only one side has Feryx + Senku, only Single Senku on that side is available.
- `LOCKED_MECHANIC`: Mechanic = blade reach extension, not a projectile wave (deliberately distinct from Arcven).
- `LOCKED_MECHANIC`: Single Senku = horizontal sweep at extended reach, not a thrust. Dual Senku = two blades sweeping diagonally, crossing into an X.
- `LOCKED_MECHANIC`: Input — tap = normal Feryx; hold = charge Senku. LMB if Feryx is in Main, RMB if Feryx is in Sub.
- `LOCKED_MECHANIC`: Feryx has no attack cost; Senku has its own Vael cost, consumed only at commit after a valid charge/release. Cancel = no cost.
- `LOCKED_MECHANIC`: Cancel paths — switch active slot, press X, dash out of animation, or interrupted by hit/stagger/knockdown/downed during charge. The charge window is intentionally vulnerable.
- `PROPOSED_DETAIL`: Single and Dual Senku costs are separate DataAsset values or an explicit formula, to be decided before implementation.
- `TUNABLE_STARTER`: `SenkuReach` starter value is around 450 cm. The original 15 m reference is a lore ceiling, not the implementation starter. All values are DataAsset-driven through `FWTBRSenkuParams` or equivalent.

## Escudo / Aegorn Wall

- `LOCKED_MECHANIC`: Place-to-deploy Trion wall. Tap = place one wall panel in front. Hold = open preset dialog, choose formation, preview, commit if valid.
- `LOCKED_MECHANIC`: Preset formations — Single, Line, Enclosure, Corner, Ramp — baked into per-panel transforms at deploy, mirroring the Viper/Serpveil preset infrastructure.
- `LOCKED_MECHANIC`: Placement is all-or-nothing — every panel must preview valid/green to place; one invalid/red panel means it cannot place; no Vael is spent on invalid placement.
- `LOCKED_MECHANIC`: Must have a supporting surface; no mid-air spawn. Each panel snaps to its own ground/wall trace, supporting slopes/stairs.
- `LOCKED_MECHANIC`: Anti-abuse — WallHP is destroyable; Duration auto-despawns; reject any spawn overlapping a pawn; no crush/stuck grief.
- `LOCKED_MECHANIC`: Escudo Slam = displacement, not damage. On spawn it pushes nearby pawns away to break position; damage = 0.
- `TUNABLE_STARTER`: `SpawnKnockbackImpulse` is DataAsset-driven — 0 = pure defensive wall, greater than 0 = displacement. Cost = PanelCount x VaelCostPerPanel (multi-panel is convenient, not cheaper). All params should live in `FWTBRAegornWallParams` or equivalent.
- `LOCKED_MECHANIC`: Server-authoritative — client sends center + rotation + formation; server computes panel transforms, validates all panels, spawns and charges Vael only if valid, otherwise rejects with no cost.

## Aegorn Shield

- `LOCKED_MECHANIC`: Cost model = upfront Vael cost + ShieldHP + Duration; not continuous drain.
- `UNKNOWN`: ShieldHP, Duration, CoverageAngle, movement/attack restrictions, and the final shield model are not locked. Do not finalize implementation before the shield model is decided.

## Voltis Trap

- `LOCKED_MECHANIC`: Voltis has a standalone Trap mode — place a trap; an enemy triggers it and is launched. Not combined with Escudo by design; any Voltis + Escudo interaction is emergent and not designed now.

## Infrastructure

- `LOCKED_MECHANIC`: Prototype listen server plus manual join.
- `PROPOSED_DETAIL`: Dedicated server later.
- `PROPOSED_DETAIL`: Matchmaking and persistence post-prototype.

