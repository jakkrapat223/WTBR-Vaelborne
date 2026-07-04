# WTBR Handoff Summary — After B8

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1 C++  
Latest baseline: 84cda95 B8: Enable corpse loot container production default
Previous handoff baseline: b287b23 Update handoff after S8B
Previous code baseline: c3dd314 Add S8B corpse container priority automation
Latest supporting asset baseline: 36b599d Add interact input asset binding
Status: B8 PASS / committed / pushed. Build PASS. `WTBR.CorpseLoot` PASS 27/27. `WTBR.Inventory` PASS 36/36. Full WTBR suite PASS 69/69. Dedicated smoke PASS.
Automation confirmed: WTBR PASS 69/69 (36 Inventory + 27 CorpseLoot + 6 DroppedTrigger), run with `Automation RunTests WTBR;Quit`. S6 BR ground-item pickup and S7A dropped-trigger F interact were PIE/manually validated end-to-end; S7A-Auto added headless C++ automation for the dropped-trigger context interact, S7B added MainOnly/SubOnly dispatch coverage, S8A added branch-3 BR ground item context-interact automation, S8B added corpse/container priority automation, B7 passed dedicated late-join validation, and B8 enabled the corpse-container production default (see S7A-Auto / S7B / S8A / S8B / B7 / B8 sections).

Competitive multiplayer gameplay must remain server-authoritative. `Source/.claude/settings.local.json` or `.claude/settings.local.json` may exist as a local-only untracked file and must never be staged or committed. Binary assets (`.uasset`/`.png`) are tracked via Git LFS.

## Recent Commits

```text
84cda95 B8: Enable corpse loot container production default
4808c0f Mark B7 dedicated late join validation pass
2461d0a Add B7D dedicated late join validation proof
6dc9f63 Update B7C validation harness results
37c9495 Add B7 dedicated validation CVar spawn harness
12d6290 Update B7 late join validation attempt results
ae481d9 Add B7 dedicated late join validation runbook
b287b23 Update handoff after S8B
c3dd314 Add S8B corpse container priority automation
```

Current baseline / `origin/main` / `origin/HEAD`: `84cda95` (B8 PASS / COMMITTED / PUSHED). Previous handoff baseline: `b287b23`. Previous completed code baseline before B8: `c3dd314`. Working tree should have no tracked changes. One local file remains intentionally uncommitted and must never be staged by an assistant without explicit approval: the ThirdPersonMap external-actor `.uasset` (`Content/__ExternalActors__/.../ThirdPersonMap/...uasset`). `Source/.claude/settings.local.json`, `.claude/settings.local.json`, `.claude/scheduled_tasks.lock`, and local validation logs may exist as local-only untracked files and must never be staged or committed.

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

- `MainOnly` / `SubOnly` dropped-trigger dispatch automation — **now covered in S7B** (see S7B section).
- Real branch-3 ground-item pickup through `RequestContextInteract` - **now covered in S8A** with a deterministic dev-automation focus seam; dedicated/PIE transport validation remains a later follow-up.

### S7B — MainOnly/SubOnly Dropped Trigger Automation — DONE / COMMITTED / PUSHED (`a1f6aa1`)

Adds the deferred MainOnly/SubOnly dropped-trigger dispatch coverage, C++-only (no Blueprint/WBP/UMG/.uasset/.umap/binary edits). DroppedTrigger automation grew from 4 → **6 tests**; full WTBR grew from 60 → **62 PASS**.

New tests in `Source/WTBR/Tests/WTBRDroppedTriggerInteractAutomationTests.cpp`:

- `WTBR.DroppedTrigger.Interact.MainOnlyDispatchesToActiveMain`
- `WTBR.DroppedTrigger.Interact.SubOnlyDispatchesToActiveSub`

Coverage is split into two deterministic phases (transient headless worlds provide no reliable network RPC transport, so these tests do not claim network-transport coverage):

- **Phase A — client routing (production path).** Drives `RequestContextInteract` and asserts, at the production routing point, the resolved dropped trigger, active target slot, and constraint. Proven: **MainOnly → active main index**, **SubOnly → active sub index**. No RPC wrapper executes and no gameplay state mutates in this phase.
- **Phase B — server-authoritative mutation.** Independently drives `AWTBRCharacter::Server_RequestPickupDroppedTrigger_Implementation` (the S7A server RPC body — not a legacy pickup shortcut) and asserts: wrong slot rejects without consuming; correct slot succeeds; the dropped actor is **consumed only on success**; only the intended active slot mutates (the other active slot is unchanged). Uses a code-only transient `UWTBRTriggerDataAsset` with a concrete `TriggerClass` (`UWTBRArcvenTrigger`) so the server replacement path completes — no `.uasset`.

Production support change (dev-only, guarded):

- `AWTBRCharacter` (`WTBRCharacter.h`/`.cpp`) — added a `#if WITH_DEV_AUTOMATION_TESTS` route-capture seam. When enabled (`SetDroppedTriggerRouteCaptureForTest`), `RequestPickupAimedDroppedTriggerByConstraint` records the focused dropped trigger, resolved slot index, and constraint/policy, then returns **without** dispatching the RPC. Default **disabled**; compiled out of shipping; **no runtime/gameplay behavior change**. Server remains authoritative; the legacy dropped-trigger path is untouched.

Existing S7A-Auto coverage remains: death-drop spawn, dropped-trigger detection, `Any` → `AmbiguousTargetSlot` rejection, and the no-focus fall-through / safety test.

Validation:

- Build: PASS (`WTBREditor Win64 Development`).
- `Automation RunTests WTBR.DroppedTrigger` → **6/6 PASS**.
- `Automation RunTests WTBR.` → **62/62 PASS**, 0 fail, 0 error.
- Staged only the 3 reviewed files (`WTBRCharacter.h`, `WTBRCharacter.cpp`, tests `.cpp`); committed `a1f6aa1`; pushed `a7e9e81..a1f6aa1`.
- The ThirdPersonMap external-actor `.uasset` remains intentionally untracked and excluded.

### S8A — Ground Item Branch 3 Context Interact Automation — DONE / COMMITTED / PUSHED (`0f43fb4`)

Adds the deferred branch-3 BR ground item context-interact automation, C++-only. No Blueprint/WBP/UMG/.uasset/.umap/binary assets were touched.

New automation in `Source/WTBR/Tests/WTBRGroundItemPickupAutomationTests.cpp`:

- `WTBR.Inventory.ContextInteract.GroundItemBranch3.PicksUpWhenFocused`
- `WTBR.Inventory.ContextInteract.GroundItemBranch3.RejectsWhenInventoryFullAndItemRemains`
- `WTBR.Inventory.ContextInteract.GroundItemBranch3.DoesNotOverrideDroppedTriggerPriority`
- `WTBR.Inventory.ContextInteract.GroundItemBranch3.DoesNotOverrideCorpseContainerPriority`

Coverage:

- Proves `RequestContextInteract` branch 3 routes to BR ground item when no higher-priority focus exists.
- Proves successful server-authoritative pickup through the existing ground item server pickup path.
- Proves inventory-full reject keeps the ground item available and does not mutate inventory.
- Proves branch 3 does not override dropped-trigger priority.
- Proves branch 3 does not override corpse/container priority.
- Proves the client/context interact path dispatches only; inventory / ground-item mutation remains on the server-approved path.

Production support change (dev/test-only, guarded):

- `UWTBRInteractionComponent` (`WTBRInteractionComponent.h` / `.cpp`) gained deterministic focused corpse-container and ground-item overrides behind `#if WITH_DEV_AUTOMATION_TESTS`.
- Default runtime behavior is unchanged. The seam is compiled out outside dev automation tests.

Validation:

- Build: PASS (`WTBREditor Win64 Development`).
- `Automation RunTests WTBR.Inventory` -> **36/36 PASS**.
- `Automation RunTests WTBR.` -> **66/66 PASS**, 0 fail, 0 error.
- Committed and pushed as `0f43fb4`.
- The ThirdPersonMap external-actor `.uasset` remains intentionally untracked and excluded.

### S8B — Corpse/container Priority Automation — DONE / COMMITTED / PUSHED (`c3dd314`)

Adds corpse/container priority automation coverage, C++ test-only. No gameplay code files were modified. No Blueprint/WBP/UMG/.uasset/.umap/binary assets were touched.

New automation in `Source/WTBR/Tests/WTBRCorpseLootAutomationTests.cpp`:

- `WTBR.CorpseLoot.ContextInteract.Priority.BeatsDroppedTrigger`
- `WTBR.CorpseLoot.ContextInteract.Priority.BeatsGroundItem`
- `WTBR.CorpseLoot.ContextInteract.Priority.DoesNotMutateLowerPriorityTargets`

Coverage:

- Added automation proving corpse/container priority 1 beats dropped trigger.
- Added automation proving corpse/container priority 1 beats BR ground item.
- Added automation proving lower-priority targets are not mutated when priority 1 wins.
- Existing `RequestContextInteract` production priority order was not changed.
- Full dedicated/PIE network transport validation is still a later follow-up.

Maintenance:

- Renamed helper functions in `WTBRGroundItemPickupAutomationTests.cpp` to avoid Unity build helper collisions.
- No gameplay code files were modified.
- No Blueprint/WBP/UMG/.uasset/.umap/binary assets were touched.

Validation:

- Build: PASS (`WTBREditor Win64 Development`).
- `Automation RunTests WTBR.CorpseLoot` -> **27/27 PASS**.
- `Automation RunTests WTBR.Inventory` -> **36/36 PASS**.
- `Automation RunTests WTBR` -> **69/69 PASS**, 0 fail, 0 error.
- Committed and pushed as `c3dd314`.
- The ThirdPersonMap external-actor `.uasset` remains intentionally untracked and excluded.

### B7 — Dedicated Server / Late Join Validation — PASS / COMMITTED / PUSHED

Dedicated server / late-join validation for corpse containers passed before the production default flip.

Key commits:

- `ae481d9` — Add B7 dedicated late join validation runbook.
- `37c9495` — Add B7 dedicated validation CVar spawn harness.
- `2461d0a` — Add B7D dedicated late join validation proof.
- `4808c0f` — Mark B7 dedicated late join validation pass.

Validation proven:

- Dedicated server starts and loads `ThirdPersonMap`.
- `IpNetDriver` listens on port `7777`.
- Client 1 joins.
- Corpse container state is spawned/maintained on the dedicated server through the B7 validation harness.
- Late-join client 2 receives correct non-empty corpse-container state.
- Client-side validation sees `Open Trigger Cache` and `RequestContextInteract handled=true`.
- Corpse/container priority remains above dropped trigger and BR ground item.
- Lower-priority dropped trigger / ground item state is not mutated when corpse/container wins.
- No Blueprint/WBP/UMG/.uasset/.umap/binary assets were touched.

### B8 — Corpse Container Production Default Flip — PASS / COMMITTED / PUSHED (`84cda95`)

B8 flips the corpse-container backend to production-default enabled after B7 passed.

Production default state:

- `WTBR.UseCorpseLootContainerOnDeath` default is now `-1`, meaning inherit/use `MatchModeRules`.
- `FWTBRMatchModeRules::bUseCorpseLootContainerOnDeath` default is now `true`.
- `AWTBRGameMode::MakeDefaultRulesForMode` generated rules now default `bUseCorpseLootContainerOnDeath = true`.
- Battle Royale match rules now enable corpse loot containers by default.
- Explicit CVar overrides are preserved: `0` still forces legacy dropped-trigger death loot; `1` still forces corpse-container death loot.
- The legacy dropped-trigger path was not removed.
- No UI, Blueprint/WBP/UMG/.uasset/.umap/binary assets were touched.

Validation:

- Build: PASS (`WTBREditor Win64 Development`).
- `Automation RunTests WTBR.CorpseLoot` -> **27/27 PASS**.
- `Automation RunTests WTBR.Inventory` -> **36/36 PASS**.
- `Automation RunTests WTBR` -> **69/69 PASS**, 0 fail, 0 error.
- Dedicated smoke: PASS (`ThirdPersonMap` loaded, `BP_WTBRGameMode_C`, `IpNetDriver` listening on port `7777`, `WTBR.UseCorpseLootContainerOnDeath = "-1" LastSetBy: Constructor`).
- Committed and pushed as `84cda95`.

## Current Pending Work

No inventory-foundation pass is in flight. The F-context dropped-trigger branch (S7A) and ground-item branch (S6) are complete; S7A-Auto + S7B provide headless automation for the dropped-trigger context interact, including MainOnly/SubOnly dispatch; S8A provides headless automation for BR ground item branch-3 context-interact routing and server-authoritative pickup/reject semantics; S8B provides corpse/container priority automation proving priority 1 beats dropped trigger and BR ground item candidates without mutating lower-priority targets. B7 passed dedicated late-join validation, and B8 enabled corpse-container backend production defaults.

**Next recommended milestone after B8:**

1. UI spec pass for Inventory / BR pickup / Loot / Quick Item / Drop flow — documentation-only first; no WBP/UMG/Blueprint/assets yet.
2. Decide whether to keep or discard the untracked ThirdPersonMap external-actor test asset.
3. Generic interactable interface / branch-4 design pass.
4. Later: full BR inventory / drop UI implementation after docs/spec pass.
5. Later: polish dedicated/PIE transport validation for ground item / corpse interaction RPC paths as the UI matures.

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
- Player-facing inventory / loot / quick item / drop UI.

## Hard Rules Reminder

- Gameplay must remain server-authoritative.
- Client/UI must not mutate inventory / slot / loot / ground item directly; the client only requests and the server validates + mutates.
- Do not edit Blueprint/WBP/UMG/.uasset/.umap binary assets unless a human explicitly approves.
- Do not hardcode physical keys (in particular the F interact key); all input bindings are default/remappable.
- Corpse-container backend production default is already enabled after B8; do not change production defaults again without explicit human approval.
- Do not remove the legacy dropped-trigger path.
- Build and automation must run from the current working tree only; do not cite old logs as current PASS.
- Do not use `git add .` / `..` / `-A`; stage only reviewed files.
- Do not stage `Source/.claude/settings.local.json` or `.claude/settings.local.json`.
- Do not stage `.claude/scheduled_tasks.lock`, validation logs, or the ThirdPersonMap external-actor `.uasset` unless a human explicitly approves.

## Remaining

- Pass F smoke audit.
- Real interact implementation: Q/E hold wheel UI and generic interactable branch remain; corpse loot, dropped trigger, and BR ground item context branches are wired.
- Inventory / BR pickup / loot / quick item / drop UI spec pass (docs-only first).
- Loot UI implementation after docs/spec approval.
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
- Real interact implementation (Q/E hold wheel and generic interactable branch).
- Dedicated/PIE transport validation for ground item pickup RPC path.
- Replication correctness.
- Match rules, travel, reset, and production defaults.

## Still Unknown Or Placeholder

- Respawn point final spec.
- Energy cube split rounding rule.
- Final Vael costs, rewards, and max pool.
- Black Trigger pool, balance, and final indicator.
- Matchmaking and persistence details.
- Dedicated server production rollout details.

## Milestone Gate Status

B7 dedicated server / late join validation passed, and B8 flipped the corpse-container production default. Future production-default changes still require explicit scope, current validation, and human approval.

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
84cda95 B8: Enable corpse loot container production default
```

`git status` may show local-only untracked files such as the intentionally excluded ThirdPersonMap external-actor `.uasset`, `.claude/settings.local.json`, `.claude/scheduled_tasks.lock`, and validation logs. None of these should be staged by an assistant without explicit human approval — in particular, do not stage the ThirdPersonMap external-actor `.uasset` (the placed ground item), local settings, lock files, or logs.

## Prompt For New Chat

```text
We are continuing WTBR / Vaelborne: Dominion UE 5.1.1 C++.

Current baseline:
84cda95 B8: Enable corpse loot container production default

Working tree should have no tracked changes. Local untracked files may include the ThirdPersonMap external-actor .uasset (the placed ground item), .claude local settings/lock files, and B7 validation logs. Do not stage any of those without explicit human approval.

Recent completed work:
- S4-A context interact dispatch foundation committed.
- S5-A..D inventory foundation committed (item data model, inventory component + TryAddItem, ground item actor + server pickup, item use + HP/Vael consumables).
- S6 BR ground item context interact wiring committed AND PIE-validated (0c4c447 expose fields, 11b9313 third-person trace fix + logs).
- S6 supporting assets committed/pushed (36b599d interact input binding, e12604a DA_Test_HP_Small, 7f50a30 starter avatar concept sheets). IA_Interact mapped to F via IMC_WTBR_Default; BP_WTBRCharacter InteractAction assigned.
- S7A dropped-trigger F context interact (branch 2) committed AND PIE-validated (c0f7c0c).
- S7A-Auto dropped-trigger context interact automation committed AND pushed (c52b484): added Source/WTBR/Tests/WTBRDroppedTriggerInteractAutomationTests.cpp with 4 tests (DeathDrop.SpawnsInMatch, Interact.FindsDroppedTrigger, Interact.AnyRejectsAmbiguousTargetSlot, Interact.NoDroppedTriggerFocusDoesNotConsumeInteract); added WITH_DEV_AUTOMATION_TESTS-only seam SpawnDroppedTriggersForEliminatedCharacterForTest() in WTBRHealthComponent.h; updated stale priority-2 comment in WTBRInteractionComponent.h. No binary/asset edits.
- S7B MainOnly/SubOnly dropped-trigger automation committed AND pushed (a1f6aa1): +2 tests (Interact.MainOnlyDispatchesToActiveMain, Interact.SubOnlyDispatchesToActiveSub) → 6 DroppedTrigger tests total. Phase A proves client routing at the production routing point (MainOnly→active main index, SubOnly→active sub index); Phase B proves server-authoritative mutation via Server_RequestPickupDroppedTrigger_Implementation (wrong slot rejects without consuming, correct slot succeeds, consumed only on success, only the intended slot mutates). Added a WITH_DEV_AUTOMATION_TESTS-only route-capture seam on AWTBRCharacter (default disabled, no shipping/runtime change). Code-only; no binary/asset edits.
- S8A ground item branch-3 context interact automation committed AND pushed (0f43fb4): added 4 WTBR.Inventory.ContextInteract.GroundItemBranch3 tests (PicksUpWhenFocused, RejectsWhenInventoryFullAndItemRemains, DoesNotOverrideDroppedTriggerPriority, DoesNotOverrideCorpseContainerPriority). Added a WITH_DEV_AUTOMATION_TESTS-only deterministic focus seam in UWTBRInteractionComponent. Proves branch-3 routing, server-authoritative pickup, full-inventory reject without mutation, and priority preservation. Code-only; no binary/asset edits.
- S8B corpse/container priority automation committed AND pushed (c3dd314): added 3 WTBR.CorpseLoot.ContextInteract.Priority tests (BeatsDroppedTrigger, BeatsGroundItem, DoesNotMutateLowerPriorityTargets). Proves RequestContextInteract priority 1 corpse/container beats dropped trigger and BR ground item candidates, and lower-priority targets do not mutate when priority 1 wins. Renamed GroundItem automation helpers to avoid Unity build helper collision. Test/docs-only; no gameplay code or binary/asset edits.
- B7 dedicated server / late-join validation committed AND pushed (4808c0f): B7 PASS. Dedicated server loaded ThirdPersonMap, IpNetDriver listened on port 7777, client 1 joined, corpse container replicated to clients, late-join client 2 observed correct non-empty corpse-container state, RequestContextInteract handled corpse/container priority, and lower-priority dropped trigger / BR ground item remained unmutated.
- B8 corpse-container production default flip committed AND pushed (84cda95): WTBR.UseCorpseLootContainerOnDeath default is now -1 (inherit/use MatchModeRules), and BattleRoyale MatchModeRules now enable corpse loot containers by default. Legacy dropped-trigger path and explicit CVar overrides remain. No UI/assets touched.

Latest automation:
Build PASS. WTBR PASS 69/69 (36 Inventory + 27 CorpseLoot + 6 DroppedTrigger). WTBR.CorpseLoot PASS 27/27. WTBR.Inventory PASS 36/36. Dedicated smoke PASS.

Important current state:
- RequestContextInteract handles corpse/container/chest (1), dropped trigger (2, S7A), and BR ground item (3, S6).
- Dropped Trigger branch 2 is implemented (S7A) and automation-covered incl. MainOnly/SubOnly dispatch (S7B): constraint-driven single-valid-target. MainOnly -> active main, SubOnly -> active sub, Any -> reject AmbiguousTargetSlot (no blind guess). Client dispatch only; server authoritative; Server_RequestPickupDroppedTrigger and legacy active main/sub paths unchanged.
- Detection uses collision-independent actor iteration (AWTBRDroppedTriggerActor has no collision/mesh): GetFocusedDroppedTrigger + FindAimedDroppedTriggerForPickup filter by distance + view-cone.
- BR Ground Item branch is implemented in S6, PIE-validated, dispatches the existing server pickup RPC/path, and is covered by S8A branch-3 context-interact automation.
- InteractionTraceDistance is 800 (third-person camera reach); server still gates pickup by WTBRGroundItemPickupRange.
- Generic interactable branch is pending interface pass (still parked).
- Corpse/container priority 1 is covered by S8B automation against dropped trigger and BR ground item candidates.
- B7 passed dedicated late-join validation, and B8 enabled corpse-container backend production defaults.
- Player-facing inventory / BR pickup / loot / quick item / drop UI is still separate and not implemented yet.
- Dev-only diagnostics available: WTBRDebugCharacterPrintTriggerSlots, WTBRDebugCharacterListNearbyDroppedTriggers, SpawnLegacyDroppedTriggers_Internal silent-return logs (all non-shipping guarded).
- AWTBRGroundItemActor exists, with a query-only Visibility-trace interaction collision; ItemData/Quantity are EditInstanceOnly.
- Server_RequestPickupGroundItem / Server_RequestUseInventoryItem / RestoreHP / UWTBRInventoryComponent exist.
- Vael Overcharge is design-locked but not implemented.
- Senku / Escudo / Tuning are design-locked/parked, not implemented.

Optional cleanup available:
- [WTBR Interact] logs are plain Log verbosity; could be gated behind Verbose/CVar/#if !UE_BUILD_SHIPPING.
- Decide version-control for the one remaining uncommitted ThirdPersonMap external-actor .uasset (placed ground item).

Start next with the recommended post-B8 milestone:
1. UI spec pass for Inventory / BR pickup / Loot / Quick Item / Drop flow - documentation-only first; no WBP/UMG/Blueprint/assets yet.
2. Decide whether to keep or discard the untracked ThirdPersonMap external-actor test asset.
3. Generic interactable interface / branch-4 design pass.
4. Later: full BR inventory / drop UI implementation after docs/spec pass.
5. Later: polish dedicated/PIE transport validation for ground item / corpse interaction RPC paths as the UI matures.
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
