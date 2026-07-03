# WTBR Handoff Summary

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Confirmed baseline: c52b484 Add S7A dropped-trigger context interact automation
Previous baseline: ead046d Update handoff after S7A dropped-trigger interact
Latest source implementation baseline: c52b484 Add S7A dropped-trigger context interact automation
Latest supporting asset baseline: 36b599d Add interact input asset binding
Automation confirmed: WTBR PASS 60/60 (32 Inventory + 24 CorpseLoot + 4 DroppedTrigger), run with `Automation RunTests WTBR.;Quit` (or `Automation RunTests WTBR;Quit`). S6 BR ground-item pickup and S7A dropped-trigger F interact were PIE/manually validated end-to-end; S7A-Auto now adds headless C++ automation coverage for the dropped-trigger context interact (see S7A-Auto section).

Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` may exist as a local-only untracked file and must never be staged or committed. Binary assets (`.uasset`/`.png`) are tracked via Git LFS.

## Recent Commits

```text
c52b484 Add S7A dropped-trigger context interact automation
ead046d Update handoff after S7A dropped-trigger interact
c0f7c0c Add S7A dropped-trigger F context interact
234d5e0 Update handoff after interact asset push
36b599d Add interact input asset binding
```

Current baseline / `origin/main` / `origin/HEAD`: `c52b484` (S7A-Auto DONE / COMMITTED / PUSHED). Previous baseline: `ead046d`. Working tree should have no tracked changes. One local file remains intentionally uncommitted and must never be staged by an assistant without explicit approval: the ThirdPersonMap external-actor `.uasset` (`Content/__ExternalActors__/.../ThirdPersonMap/...uasset`).

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

### S7A — Dropped Trigger F Context Interact (branch 2) — DONE / COMMITTED / PUSHED (`c0f7c0c`)

Dropped Trigger F context interact branch 2 is implemented. `RequestContextInteract` priority order is now:

1. corpse/container/chest — implemented
2. **dropped trigger — implemented (S7A)**
3. BR ground item — implemented (S6)
4. generic interactable — parked
5. no-op

**S7A Policy (constraint-driven single-valid-target):**

- `MainOnly` → dispatch into active main slot (`GetActiveMainIndex()`).
- `SubOnly` → dispatch into active sub slot (`GetActiveSubIndex()`).
- `Any` → reject as `AmbiguousTargetSlot` (dev-log only); no dispatch.
- No blind Main/Sub guess when a trigger has two valid active targets (`Any`).
- Client dispatch only — the client never mutates slots/inventory/actors.
- Server remains authoritative; it re-validates before any mutation.
- `Server_RequestPickupDroppedTrigger` signature and its mutation/swap (transactional consume → replace → rollback) behavior are **unchanged**.
- Legacy `RequestPickupAimedDroppedTriggerIntoActiveMainSlot` / `...IntoActiveSubSlot` pickup paths are **preserved**.
- Resolver entry point: `AWTBRCharacter::RequestPickupAimedDroppedTriggerByConstraint()`; component branch: `UWTBRInteractionComponent::GetFocusedDroppedTrigger()`.

**Detection fix:**

- `AWTBRDroppedTriggerActor` has **no collision primitive and no mesh component** (bare `USceneComponent` root).
- Therefore line trace / channel sweep could not reliably find dropped-trigger actors (floor blocked the trace; sweep returned nothing).
- `GetFocusedDroppedTrigger` and `FindAimedDroppedTriggerForPickup` now use **collision-independent actor iteration** with distance + view-cone (dot ≥ 0.5) filtering.
- This is detection/dispatch only — not client-side mutation. Server still re-validates distance on pickup.

**Dev-only diagnostics added (guarded, non-shipping):**

- `WTBRDebugCharacterPrintTriggerSlots` — dumps server-side trigger loadout (roles, match gate, active main/sub, per-slot DataAsset/Category/SlotConstraint).
- `WTBRDebugCharacterListNearbyDroppedTriggers` — lists dropped-trigger actors with location, distance from char/camera, DataAsset, IsConsumed, root collision info.
- `SpawnLegacyDroppedTriggers_Internal` silent-return diagnostic logs (invalid victim / no world / match-gate values / no TriggerSetComponent / 0 snapshots / per-slot skip).

**Validation:**

- Build: PASS (`WTBREditor Win64 Development`).
- Automation: `Automation RunTests WTBR;Quit` → PASS **56/56**, 0 FAIL.
- Manual PIE core: **PASS** —
  - `WTBRDebugCharacterSetMatchPhase InMatch` set match phase to InMatch (default is `LoadoutSetup`; no auto-transition).
  - Legacy dropped-trigger death-drop spawned 8 actors (`WTBR.UseCorpseLootContainerOnDeath 0`).
  - `WTBRDebugCharacterListNearbyDroppedTriggers` confirmed the actors exist.
  - Interact reached dropped trigger priority 2 (`... focused (priority 2); routing to RequestPickupAimedDroppedTriggerByConstraint`).
  - Current DataAssets are `SlotConstraint=Any` → resolver correctly rejected as `AmbiguousTargetSlot`; trigger stayed on the ground.
  - BR Ground Item priority 3 regression pickup still succeeded.
- Corpse/container-vs-dropped-trigger priority smoke test: **optional / not re-run** (accepted).

### S7A-Auto — Dropped Trigger Context Interact Automation — DONE / COMMITTED / PUSHED (`c52b484`)

Converts the repeated Manual PIE checks for the dropped-trigger context interact into headless C++ automation. Low-risk, code-only: **no Blueprint/WBP/UMG/.uasset/.umap/binary asset edits**.

Added `Source/WTBR/Tests/WTBRDroppedTriggerInteractAutomationTests.cpp` with 4 tests (all run in a transient `EWorldType::Game` world with an authority character + in-match `AWTBRGameState`; detection driven off `GetActorEyesViewPoint`, no PIE camera / no physics scene):

1. `WTBR.DroppedTrigger.DeathDrop.SpawnsInMatch` — in-match + legacy path (`WTBR.UseCorpseLootContainerOnDeath 0`) + installed snapshot > 0 → death-drop spawns `AWTBRDroppedTriggerActor` (> 0).
2. `WTBR.DroppedTrigger.Interact.FindsDroppedTrigger` — `RequestContextInteract` detects + routes to a dropped trigger placed in view; control: once consumed it is no longer routed.
3. `WTBR.DroppedTrigger.Interact.AnyRejectsAmbiguousTargetSlot` — `Any` constraint → resolver rejects as `AmbiguousTargetSlot` (asserted via the expected warning + externally-visible effects: actor stays unconsumed, active main/sub indices unchanged, no dispatch/mutation). No public result enum exists, so external effects are the contract.
4. `WTBR.DroppedTrigger.Interact.NoDroppedTriggerFocusDoesNotConsumeInteract` — deterministic branch-precedence guard: with nothing focusable, priority-2 does **not** consume the interact (`RequestContextInteract` returns false / falls through). Renamed from the original over-claiming `DoesNotBreakGroundItemBranch`; its TODO documents that full branch-3 ground-item pickup through `RequestContextInteract` is deferred (needs a reliable `ECC_Visibility` / physics-backed PIE or fixture world), that S6 `WTBR.Inventory.Pickup.*` still covers `Server_RequestPickupGroundItem` semantics, and that this test only proves priority-2 non-consumption.

Support / doc changes (code-only):

- `Source/WTBR/Components/WTBRHealthComponent.h` — added a `#if WITH_DEV_AUTOMATION_TESTS` test-only seam `SpawnDroppedTriggersForEliminatedCharacterForTest()` (compiled out of shipping) that invokes the existing server-side drop routine directly, mirroring the existing `UWTBRTriggerSetComponent::SetSlotDataAssetForTest` convention. The full damage→downed→eliminated pipeline is timer/CoreStats-asset dependent and not headless-safe, so it is not driven. Runtime behavior unchanged.
- `Source/WTBR/Components/WTBRInteractionComponent.h` — updated the stale priority-2 comment (was "parked: needs active-main/sub target-slot policy") to the implemented S7A policy: MainOnly → active main, SubOnly → active sub, Any → reject `AmbiguousTargetSlot`. Comment-only; no runtime logic change.

Validation:

- Build: PASS (`WTBREditor Win64 Development`).
- `Automation RunTests WTBR.DroppedTrigger` → **4/4 PASS**, 0 fail, 0 error.
- `Automation RunTests WTBR.` → **60/60 PASS**, 0 fail, 0 error.
- Staged only the 3 reviewed files (M HealthComponent.h, M InteractionComponent.h, A tests .cpp); committed `c52b484`; pushed `ead046d..c52b484`.
- The ThirdPersonMap external-actor `.uasset` remains intentionally untracked and excluded from the change set.

Deferred (reported, no assets created/modified):

- `MainOnly` / `SubOnly` dropped-trigger dispatch automation — feasible code-only but exercises the mutating `Server_RequestPickupDroppedTrigger` path; left out to keep this pass to the non-mutating subset.
- Real branch-3 ground-item pickup through `RequestContextInteract` — needs a physics-backed visibility-trace fixture (see test 4 TODO).

## Current Pending Work

No inventory-foundation pass is in flight. The F-context dropped-trigger branch (S7A) and ground-item branch (S6) are complete, and S7A-Auto has added headless automation coverage for the dropped-trigger context interact.

**Next candidates after S7A-Auto:**

1. MainOnly/SubOnly dropped-trigger automation fixture investigation (mutating `Server_RequestPickupDroppedTrigger` dispatch path).
2. Real branch-3 ground-item pickup through `RequestContextInteract` with a reliable physics/visibility-trace fixture (PIE or physics-backed world).
3. Corpse/container priority automation or smoke test.
4. B7 dedicated server / late-join validation before the corpse-container production-default flip.
5. Decide whether to keep or discard the untracked ThirdPersonMap external-actor test asset.

F context priority (current status):

1. corpse/container/chest — implemented
2. dropped trigger — implemented (S7A)
3. BR ground item — implemented (S6)
4. generic interactable — parked (needs interface pass)
5. no-op

## Parked / Not Implemented

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
- Client/UI must not mutate inventory / slot / loot / ground item directly; the client only requests and the server validates + mutates.
- Do not edit Blueprint/WBP/UMG/.uasset/.umap binary assets unless a human explicitly approves.
- Do not hardcode physical keys (in particular the F interact key); all input bindings are default/remappable.
- Do not flip corpse container production default until B7 dedicated/late-join validation passes.
- Do not remove the legacy dropped-trigger path.
- Do not use `git add .` / `..` / `-A`; stage only reviewed files.
- Do not stage `Source/.claude/settings.local.json`.
- Do not stage the ThirdPersonMap external-actor `.uasset` unless a human explicitly approves.

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

Expected latest baseline (at `HEAD` / `main` / `origin/main` / `origin/HEAD`):

```text
c52b484 Add S7A dropped-trigger context interact automation
```

`git status` should show only the intentionally excluded ThirdPersonMap external-actor `.uasset` as untracked. `Source/.claude/settings.local.json` may exist as a local-only untracked file. Both must never be staged by an assistant without explicit approval — in particular, do not stage the ThirdPersonMap external-actor `.uasset` (the placed ground item).

## Prompt For New Chat

```text
We are continuing WTBR / Vaelborne: Dominion UE 5.1.1 C++.

Current baseline:
c52b484 Add S7A dropped-trigger context interact automation

Working tree should have no tracked changes. One local file remains intentionally uncommitted and must never be staged by an assistant without explicit approval: the ThirdPersonMap external-actor .uasset (the placed ground item).

Recent completed work:
- S4-A context interact dispatch foundation committed.
- S5-A..D inventory foundation committed (item data model, inventory component + TryAddItem, ground item actor + server pickup, item use + HP/Vael consumables).
- S6 BR ground item context interact wiring committed AND PIE-validated (0c4c447 expose fields, 11b9313 third-person trace fix + logs).
- S6 supporting assets committed/pushed (36b599d interact input binding, e12604a DA_Test_HP_Small, 7f50a30 starter avatar concept sheets). IA_Interact mapped to F via IMC_WTBR_Default; BP_WTBRCharacter InteractAction assigned.
- S7A dropped-trigger F context interact (branch 2) committed AND PIE-validated (c0f7c0c).
- S7A-Auto dropped-trigger context interact automation committed AND pushed (c52b484): added Source/WTBR/Tests/WTBRDroppedTriggerInteractAutomationTests.cpp with 4 tests (DeathDrop.SpawnsInMatch, Interact.FindsDroppedTrigger, Interact.AnyRejectsAmbiguousTargetSlot, Interact.NoDroppedTriggerFocusDoesNotConsumeInteract); added WITH_DEV_AUTOMATION_TESTS-only seam SpawnDroppedTriggersForEliminatedCharacterForTest() in WTBRHealthComponent.h; updated stale priority-2 comment in WTBRInteractionComponent.h. No binary/asset edits.

Latest automation:
WTBR PASS 60/60 (32 Inventory + 24 CorpseLoot + 4 DroppedTrigger).

Important current state:
- RequestContextInteract handles corpse/container/chest (1), dropped trigger (2, S7A), and BR ground item (3, S6).
- Dropped Trigger branch 2 is implemented (S7A): constraint-driven single-valid-target. MainOnly -> active main, SubOnly -> active sub, Any -> reject AmbiguousTargetSlot (no blind guess). Client dispatch only; server authoritative; Server_RequestPickupDroppedTrigger and legacy active main/sub paths unchanged.
- Detection uses collision-independent actor iteration (AWTBRDroppedTriggerActor has no collision/mesh): GetFocusedDroppedTrigger + FindAimedDroppedTriggerForPickup filter by distance + view-cone.
- BR Ground Item branch is implemented in S6, PIE-validated, dispatches the existing server pickup RPC/path.
- InteractionTraceDistance is 800 (third-person camera reach); server still gates pickup by WTBRGroundItemPickupRange.
- Generic interactable branch is pending interface pass (still parked).
- Dev-only diagnostics available: WTBRDebugCharacterPrintTriggerSlots, WTBRDebugCharacterListNearbyDroppedTriggers, SpawnLegacyDroppedTriggers_Internal silent-return logs (all non-shipping guarded).
- AWTBRGroundItemActor exists, with a query-only Visibility-trace interaction collision; ItemData/Quantity are EditInstanceOnly.
- Server_RequestPickupGroundItem / Server_RequestUseInventoryItem / RestoreHP / UWTBRInventoryComponent exist.
- Vael Overcharge is design-locked but not implemented.
- Senku / Escudo / Tuning are design-locked/parked, not implemented.

Optional cleanup available:
- [WTBR Interact] logs are plain Log verbosity; could be gated behind Verbose/CVar/#if !UE_BUILD_SHIPPING.
- Decide version-control for the one remaining uncommitted ThirdPersonMap external-actor .uasset (placed ground item).

Start next with one of the S7A-Auto follow-up candidates:
1. MainOnly/SubOnly dropped-trigger automation fixture investigation (mutating Server_RequestPickupDroppedTrigger dispatch path).
2. Real branch-3 ground-item pickup through RequestContextInteract with a reliable physics/visibility-trace fixture.
3. Corpse/container priority automation or smoke test.
4. B7 dedicated server / late-join validation before the corpse-container production-default flip.
5. Decide whether to keep or discard the untracked ThirdPersonMap external-actor test asset.
Keep gameplay server-authoritative and run WTBR automation after changes.
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
