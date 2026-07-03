# WTBR Handoff Summary

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: 36b599d Add interact input asset binding
Latest source implementation baseline: 11b9313 Fix third-person ground item focus trace and add interact logs
Latest supporting asset baseline: 36b599d Add interact input asset binding
Automation confirmed: WTBR.Inventory + WTBR.CorpseLoot PASS 56/56 (32 Inventory + 24 CorpseLoot), run with `Automation RunTests WTBR.Inventory+WTBR.CorpseLoot;Quit`. S6 BR ground-item pickup was also **PIE/manually validated** end-to-end (see S6 section).

Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` may exist as a local-only untracked file and must never be staged or committed. Binary assets (`.uasset`/`.png`) are tracked via Git LFS.

## Recent Commits

```text
36b599d Add interact input asset binding
e12604a Add HP consumable test data asset
7f50a30 Add starter avatar concept sheets
a114d82 Update handoff after S6 PIE validation
11b9313 Fix third-person ground item focus trace and add interact logs
```

Current baseline / `origin/main`: `36b599d`. Working tree should have no tracked changes. One local file remains intentionally uncommitted and must never be staged by an assistant without explicit approval: the ThirdPersonMap external-actor `.uasset` (`Content/__ExternalActors__/.../ThirdPersonMap/...uasset`).

## Repo Paths

- PC: `E:\World Trigger\WTBR-Vaelborne`
- Notebook: `C:\Trigger\WTBR-Vaelborne`

## Module And Build Layout

- Module: `WTBR`
- Build.cs: `Source/WTBR/WTBR.Build.cs`
- Tests: `Source/WTBR/Tests/`

## Done

- Corpse loot automation foundation.
- Corpse loot container tests. WTBR.CorpseLoot PASS 12/12.
- Same-container occupied-slot swap fixed (5afa7bb).
- Corpse loot interact request bridge: `UWTBRInteractionComponent`, `RequestCorpseLootInteract()`, `OnCorpseLootInteractRequested` delegate (416e4e3).
- B1-B6 multiplayer validation PASS.
- Nexil cleanup fix PASS.
- Human Test Gate v1.1 committed (ed9ecf0).
- Input design lock documented: Q/E tap = cycle slot; Q/E hold = wheel; F = context interact via `IA_Interact`/`InteractAction`.

## Completed — Inventory Foundation (S4-A, S5-A..D)

### S4-A — F Context Interact Dispatch Foundation — DONE / committed / pushed (6b106ce)

- `UWTBRInteractionComponent::RequestContextInteract()` exists.
- `AWTBRCharacter::Interact()` routes through `RequestContextInteract()`.
- Currently implemented priority: corpse/container/chest + BR ground item (branch 3, added in S6).
- Dropped Trigger branch remains pending due to target slot policy.
- BR Ground Item branch is implemented in S6 (see below).
- Generic interactable branch remains pending until interface pass.

### S5-A — BR Inventory Item Data Model — DONE / committed / pushed (de6be18)

- Added `UWTBRItemDataAsset` (`EWTBRItemType`, `EWTBRConsumableEffectType`).
- Added `FWTBRInventorySlot`.
- No behavior / pickup / use.

### S5-B — Inventory Component + Transactional TryAddItem — DONE / committed / pushed (77b67d8)

- Added `UWTBRInventoryComponent`.
- Replicated inventory slots.
- `TryAddItem` is server-authoritative.
- Stacking is all-or-nothing: partial stack first → first empty slot → reject if full.
- Inventory automation added.

### S5-C — Ground Item Actor + Server Pickup — DONE / committed / pushed (886c937)

- Added `AWTBRGroundItemActor`.
- Added `UWTBRInventoryComponent` to `AWTBRCharacter`.
- Added `AWTBRCharacter::Server_RequestPickupGroundItem`.
- Pickup uses `IsInMatch()` MVP gate; does not reuse trigger pickup/swap gates.
- Ground item pickup is all-or-nothing.
- `RequestContextInteract` was not wired to ground items yet.

### S5-D — Inventory Item Use + HP/Vael Consumables — DONE / committed / pushed (a05195f)

- Added `UWTBRHealthComponent::RestoreHP` (alive-only, clamps to MaxHP, no revive/overheal).
- Added `UWTBRInventoryComponent::ConsumeItemAtSlot` (server-only consume support).
- Added `AWTBRCharacter::Server_RequestUseInventoryItem`.
- `HealHP` uses `RestoreHP`; `RestoreVael` uses `VaelComponent::GrantVael`.
- Unsupported effects (e.g. `VaelOvercharge`) are rejected without consuming.
- Item is consumed only if the effect actually succeeds.
- No VaelOvercharge implementation. No tuning/Senku/Escudo. No `RequestContextInteract` wiring.

### S6 — BR Ground Item Context Interact Wiring — DONE / committed / pushed / PIE-validated

Commits: `4a2c459` (wiring) → `0c4c447` (expose fields) → `11b9313` (third-person trace fix + logs).

- `AWTBRGroundItemActor` now has an asset-free, query-only `USphereComponent` interaction collision (`4a2c459`).
- Collision is `QueryOnly`, `ECC_WorldDynamic`, ignores all channels, and blocks `ECC_Visibility` only.
- It does not block movement, physics, or projectiles, and generates no overlap events.
- `UWTBRInteractionComponent` now has a focused-ground-item line-trace helper (`GetFocusedGroundItem`, mirrors the corpse/container focus trace).
- `RequestContextInteract()` branch 3 (BR ground item) now dispatches pickup to the existing `AWTBRCharacter::Server_RequestPickupGroundItem`.
- Corpse/container/chest priority remains first.
- Dropped-trigger F branch remains intentionally parked (no locked target-slot policy).
- Generic interactable branch remains parked.
- Server authority model unchanged: the client only requests; the server validates and decides.
- `0c4c447` — exposed `AWTBRGroundItemActor::ItemData` and `Quantity` as `EditInstanceOnly` so a hand-placed ground item can be configured in the editor Details panel (needed for manual PIE). Runtime init / replication / server validation unchanged.
- `11b9313` — increased `InteractionTraceDistance` 300 → 800 so the third-person follow camera (behind a ~400-unit spring arm) can reach items at/in front of the pawn; added concise `[WTBR Interact]` diagnostic logs. Server still gates pickup independently by `WTBRGroundItemPickupRange`.

Validation:

- Build: PASS (`WTBREditor Win64 Development`).
- Automation command: `Automation RunTests WTBR.Inventory+WTBR.CorpseLoot;Quit`.
- Result: 56/56 PASS, 0 FAIL (32 Inventory + 24 CorpseLoot) — held across `4a2c459`, `0c4c447`, `11b9313`.
- PIE/manual: **PASS**. After `WTBRDebugCharacterSetMatchPhase InMatch`, aiming at a placed ground item and pressing F produced:
  `RequestContextInteract called` → `Ground focus trace hit WTBRGroundItemActor isGroundItem=true at 439/800` →
  `dispatching Server_RequestPickupGroundItem` → `WTBR ground item pickup succeeded`.
- PIE-setup assets are now committed/pushed (`36b599d`, `e12604a`, `7f50a30`):
  - `Content/Input/Actions/IA_Interact.uasset` — created; mapped to default **F** via `IMC_WTBR_Default`.
  - `Content/Input/IMC_WTBR_Default.uasset` — `IA_Interact` → F binding.
  - `Content/Blueprints/BP_WTBRCharacter.uasset` — `InteractAction` assigned to `IA_Interact` (temporary debug Print String removed before commit).
  - `Content/Data/AssetTest/DA_Test_HP_Small.uasset` — permanent manual PIE / consumable-heal test fixture (NOT final balance data).
  - `Source/output/concepts/starter_avatars/*.png` — starter avatar concept sheets, committed as reference assets.
- The ThirdPersonMap external-actor `.uasset` (the placed ground item) remains **intentionally uncommitted** unless later approved.

## Current Pending Work

No inventory-foundation pass is in flight. The F-context ground-item branch is complete; the next interaction work is the still-parked list below.

F context priority (current status):

1. corpse/container/chest — implemented
2. dropped trigger — parked (no locked target-slot policy: active main? active sub? slot-selection UI?)
3. BR ground item — implemented (S6)
4. generic interactable — parked (needs interface pass)
5. no-op

## Parked / Not Implemented

- Dropped-trigger F pickup branch (needs target-slot policy).
- Generic interactable interface.
- Senku implementation.
- Escudo / Aegorn Wall implementation.
- Aegorn Shield implementation.
- Vael Overcharge effect.
- Trigger Tuning attach/swap.
- Player custom preset editor.
- Composite UI.
- Q/E hold final UI assets.
- Final bag / inventory UI.
- B7 dedicated server / late join validation.
- Corpse container production default flip (gated on B7).

## Hard Rules Reminder

- Gameplay must remain server-authoritative.
- Do not edit Blueprint/WBP/UMG/.uasset/.umap binary assets.
- Do not hardcode physical keys; all input bindings are default/remappable.
- Do not flip corpse container production default until B7 dedicated/late-join validation passes.
- Do not remove the legacy dropped-trigger path.
- Do not use `git add .` / `..` / `-A`; stage only reviewed files.

## Remaining

- Pass F smoke audit.
- B7 dedicated server / late join.
- Real interact implementation: Q/E hold wheel UI; context interact dispatch (ground item pickup, generic interactable) not yet wired to `InteractAction`; corpse loot path done.
- Loot UI.
- Match flow.
- Composite input/UI.
- Aegorn completion.
- Nexil destructible wire.
- Shrinking zone.
- Capture-zone Black Trigger objective.
- Down/Revive/Respawn.
- Loadout preset feature.
- Cube split multi-projectile model.

## Testability Map

Automation-testable pure logic:

- Corpse loot struct/predicate behavior.
- Corpse loot container entry filtering, ordering, consumed state, rollback, prompt/availability, all-consumed state, zero lifetime, and positive lifetime CVar behavior.
- Deterministic rule helpers and pure data transformations.

S2/S3 PIE/network required items:

- Full `AWTBRCharacter::Server_RequestPickupCorpseLootEntry` path.
- Invalid target slot through RPC.
- `ReplaceTriggerSlotFromDataAsset` end-to-end.
- Destroy-after-last-pickup through character flow.
- Focused trace end-to-end.
- Real interact implementation (Q/E hold wheel, context dispatch to ground item / generic interactable).
- Replication correctness.
- Dedicated server / late join.
- Match rules, travel, reset, and production defaults.

## Still Unknown Or Placeholder

- Respawn point final spec.
- Energy cube split rounding rule.
- Final Vael costs, rewards, and max pool.
- Black Trigger pool, balance, and final indicator.
- Matchmaking and persistence details.
- Dedicated server production rollout details.

## Hard Gate

B7 dedicated server / late join must pass before flipping the corpse-container production default.

## Human Test Gate

Before requesting any Human PIE/manual test, apply Human Test Gate v1.1 from `Docs/WTBR_WORKFLOW_POLICY.md`:

- Gate 1: changed path filter using `git diff --name-only <last-validated-baseline> --` and `git status --short -uall`.
- Gate 2: relevant automation against the current working tree.
- Gate 3: human-only remainder for changed paths not provable by automation.

The assistant/AI must restate this gate before asking for manual PIE. Human checklists must show which cases were removed by Gate 1 and Gate 2.

## Clear Before New Chat

Run:

```powershell
git status --short -uall
git log --oneline --decorate -5
```

Expected latest baseline:

```text
36b599d Add interact input asset binding
```

`Source/.claude/settings.local.json` may exist as a local-only untracked file. One local file remains intentionally uncommitted and must never be staged by an assistant without explicit approval: the ThirdPersonMap external-actor `.uasset` (the placed ground item).

## Prompt For New Chat

```text
We are continuing WTBR / Vaelborne: Dominion UE 5.1.1 C++.

Current baseline:
36b599d Add interact input asset binding

Working tree should have no tracked changes. One local file remains intentionally uncommitted and must never be staged by an assistant without explicit approval: the ThirdPersonMap external-actor .uasset (the placed ground item).

Recent completed work:
- S4-A context interact dispatch foundation committed.
- S5-A item data model committed.
- S5-B inventory component + transactional TryAddItem committed.
- S5-C ground item actor + server pickup committed.
- S5-D inventory item use + HP/Vael consumables committed.
- S6 BR ground item context interact wiring committed AND PIE-validated (0c4c447 expose fields, 11b9313 third-person trace fix + logs).
- S6 supporting assets committed/pushed (36b599d interact input binding, e12604a DA_Test_HP_Small, 7f50a30 starter avatar concept sheets). IA_Interact mapped to F via IMC_WTBR_Default; BP_WTBRCharacter InteractAction assigned.

Latest automation:
WTBR.Inventory + WTBR.CorpseLoot PASS 56/56.

Important current state:
- RequestContextInteract exists. It handles corpse/container/chest and BR ground item (branch 3).
- BR Ground Item branch is implemented in S6, PIE-validated, and dispatches the existing server pickup RPC/path.
- InteractionTraceDistance is 800 (third-person camera reach); server still gates pickup by WTBRGroundItemPickupRange.
- Dropped Trigger branch is pending due to target slot ambiguity (still parked).
- Generic interactable branch is pending interface pass (still parked).
- AWTBRGroundItemActor exists, with a query-only Visibility-trace interaction collision; ItemData/Quantity are EditInstanceOnly.
- Server_RequestPickupGroundItem exists.
- UWTBRInventoryComponent exists.
- Server_RequestUseInventoryItem exists.
- RestoreHP exists.
- Vael Overcharge is design-locked but not implemented.
- Senku / Escudo / Tuning are design-locked/parked, not implemented.

Optional cleanup available:
- [WTBR Interact] logs are plain Log verbosity; could be gated behind Verbose/CVar/#if !UE_BUILD_SHIPPING.
- Decide version-control for the one remaining uncommitted ThirdPersonMap external-actor .uasset (placed ground item).

Start next with:
Choose the next parked interaction item (dropped-trigger target-slot policy, or generic interactable interface) — or a non-interaction pass.
Keep gameplay server-authoritative and run WTBR.Inventory + WTBR.CorpseLoot after changes.
```

## Safe Commit And Push Pattern

```powershell
git status --short -uall
git diff -- <specific files>
git add <specific files only>
git diff --cached --stat
git diff --cached
git commit -m "<reviewed message>"
```

Push only after explicit human instruction:

```powershell
git push
```
