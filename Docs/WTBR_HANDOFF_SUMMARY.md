# WTBR Handoff Summary

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: a05195f Add inventory consumable use foundation
Automation confirmed: WTBR.Inventory + WTBR.CorpseLoot PASS 56/56 (32 Inventory + 24 CorpseLoot). This was run before the S5-D commit on the committed working tree; it was not rerun during the docs-only handoff update.

Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` may exist as a local-only untracked file and must never be staged or committed.

## Recent Commits

```text
a05195f Add inventory consumable use foundation
886c937 Add ground item pickup foundation
77b67d8 Add inventory component stacking foundation
de6be18 Add BR inventory item data model
6b106ce Add context interact dispatch foundation
```

Current baseline / `origin/main`: `a05195f`. Working tree should be clean.

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
- Currently implemented priority: corpse/container/chest path only.
- Dropped Trigger branch remains pending due to target slot policy.
- BR Ground Item branch remains pending until S6 wiring.
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

## Current Pending Work

Next recommended pass: **S6 — Wire BR Ground Item into `RequestContextInteract` branch 3.**

S6 scope:

- Detect focused `AWTBRGroundItemActor`.
- Call `Server_RequestPickupGroundItem`.
- Preserve existing corpse/container priority.
- Do not implement item use UI.
- Do not implement dropped-trigger F branch.
- Do not implement generic interactable interface.
- Do not implement Senku / Escudo / Tuning / VaelOvercharge.

F context priority remains:

1. corpse/container/chest
2. dropped trigger
3. BR ground item
4. generic interactable
5. no-op

Priority 2 (dropped trigger) is intentionally not implemented because a single F press has no locked target-slot policy (active main? active sub? slot-selection UI?). S6 should wire only the BR Ground Item branch safely without guessing dropped-trigger slot behavior.

## Parked / Not Implemented

- Dropped-trigger F pickup branch (needs target-slot policy).
- Generic interactable interface.
- Senku implementation.
- Escudo / Aegorn Wall implementation.
- Aegorn Shield implementation.
- Vael Overcharge effect.
- Trigger Tuning attach/swap.
- Player custom preset editor.
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
a05195f Add inventory consumable use foundation
```

`Source/.claude/settings.local.json` may exist as a local-only untracked file.

## Prompt For New Chat

```text
We are continuing WTBR / Vaelborne: Dominion UE 5.1.1 C++.

Current baseline:
a05195f Add inventory consumable use foundation

Working tree should be clean.

Recent completed work:
- S4-A context interact dispatch foundation committed.
- S5-A item data model committed.
- S5-B inventory component + transactional TryAddItem committed.
- S5-C ground item actor + server pickup committed.
- S5-D inventory item use + HP/Vael consumables committed.

Latest automation:
WTBR.Inventory + WTBR.CorpseLoot PASS 56/56.

Important current state:
- RequestContextInteract exists. It currently handles corpse/container/chest path.
- Dropped Trigger branch is pending due to target slot ambiguity.
- BR Ground Item branch is pending S6 wiring.
- Generic interactable branch is pending interface pass.
- AWTBRGroundItemActor exists.
- Server_RequestPickupGroundItem exists.
- UWTBRInventoryComponent exists.
- Server_RequestUseInventoryItem exists.
- RestoreHP exists.
- Vael Overcharge is design-locked but not implemented.
- Senku / Escudo / Tuning are design-locked/parked, not implemented.

Start next with:
S6 Planning/Implementation — wire BR Ground Item pickup into RequestContextInteract branch 3 only.

S6 constraints:
- Do not implement dropped-trigger branch.
- Do not guess main/sub target slot.
- Do not implement generic interactable.
- Do not implement UI.
- Do not edit .uasset/WBP/Blueprint.
- Keep gameplay server-authoritative.
- Run WTBR.Inventory + WTBR.CorpseLoot after changes.
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
