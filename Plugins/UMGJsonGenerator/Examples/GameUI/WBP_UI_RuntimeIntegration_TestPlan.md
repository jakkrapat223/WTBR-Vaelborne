# WBP UI Runtime Integration — Phase 5A / 6A Test Plan

Phase 5A (docs-only): no `.uasset`/`.umap`/plugin-source changes, no widget-spawn code.
Prepares the next Unreal Editor / PIE session for a human to assign WBP parent classes,
compile, and smoke-test HUD + Bag/Loot binding.

Phase 6A (this update): adds the minimal runtime C++ scaffold so a locally controlled
`AWTBRCharacter` can create the generated HUD and Bag/Loot widgets at runtime. Still no
`.uasset`/`.umap`/plugin-source changes, no JSON import, no WBP asset opened/saved.

---

## 1. Current UI asset status

| Asset | Status |
|---|---|
| `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_HUD_Generated.json` | Complete, accepted as usable mockup |
| `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagPanel_Generated.json` | Complete, accepted as usable mockup |
| `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_CorpseLootPanel_Generated.json` | Complete, accepted as usable mockup |
| `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagLootLayer_Generated.json` | Complete, accepted as usable mockup |
| Native parent class for HUD | [`UWTBRGeneratedHUDWidget`](../../../../Source/WTBR/UI/WTBRGeneratedHUDWidget.h) — exists, builds clean |
| Native parent class for Bag/Loot | [`UWTBRBagLootWidget`](../../../../Source/WTBR/UI/WTBRBagLootWidget.h) — exists, builds clean, shared by all three Bag/Loot WBPs |
| ViewModel components | [`UWTBRHUDViewModelComponent`](../../../../Source/WTBR/Components/WTBRHUDViewModelComponent.h), [`UWTBRBagLootViewModelComponent`](../../../../Source/WTBR/Components/WTBRBagLootViewModelComponent.h) — both owned by `AWTBRCharacter`, both build clean |

Build status: Development Editor Win64, 0 errors / 0 warnings (per session state at start of this pass; no C++ was touched in this pass, so this is unchanged).

---

## 2. Manual Unreal setup steps (future human session)

**HUD:**
1. Import latest `WBP_HUD_Generated.json` if the widget tree needs refreshing.
2. Open `WBP_HUD_Generated`.
3. Class Settings → Parent Class = `WTBRGeneratedHUDWidget`.
4. Compile.
5. Save only `WBP_HUD_Generated`.

**Bag/Loot:**
1. Import latest `WBP_BagLootLayer_Generated.json` if needed.
2. Open `WBP_BagLootLayer_Generated`.
3. Class Settings → Parent Class = `WTBRBagLootWidget`.
4. Compile.
5. Save only `WBP_BagLootLayer_Generated`.

**Phase 6A — assign widget classes (new):**
8. Assign `HUDWidgetClass` (target: `/Game/UI/Generated/WBP_HUD_Generated`) and `BagLootWidgetClass` (target: `/Game/UI/Generated/WBP_BagLootLayer_Generated`) on the `AWTBRCharacter` Blueprint default (or CDO) — do this only *after* steps 1-7 above, so the WBPs already have their native parent class set. See §9 for the full Phase 6A scaffold.
9. Do not "Save All".
10. Do not save maps/external actors.

**Optional:**
- `WBP_BagPanel_Generated` and `WBP_CorpseLootPanel_Generated` can also have their Parent Class set to `WTBRBagLootWidget` — every binding uses `BindWidgetOptional`, so a WBP that only contains the Bag widgets (or only the Corpse Loot widgets) still compiles cleanly.

**Safety:**
- Do not "Save All".
- Do not save maps.
- Do not save external actors.
- Save only the specific reviewed generated WBP asset just edited.

---

## 3. Runtime spawn flow audit (findings)

Searched: PlayerController, HUD class, GameMode/GameState, Character `BeginPlay`/`PossessedBy`/`OnRep_PlayerState`, `CreateWidget`/`AddToViewport`/`RemoveFromParent`, existing HUD widgets, inventory/interaction open-close logic, input actions for bag/interact/cancel.

| Question | Finding |
|---|---|
| Is there already a runtime place that creates HUD widgets? | **No** (as of Phase 5A). No `CreateWidget<...>()` or `AddToViewport()` call existed anywhere in `Source/WTBR`. **Superseded by Phase 6A** (§9) — `AWTBRCharacter::CreateLocalPlayerUI()` now fills this gap as a temporary Character-owned scaffold. |
| Is there already an existing HUD WBP class reference? | **No.** `AWTBRGameMode` (`WTBR/WTBRGameMode.h/.cpp`) sets `DefaultPawnClass` only (`WTBRGameMode.cpp:59`). No `HUDClass` or `PlayerControllerClass` override found anywhere in Source. No custom `AHUD` subclass exists in the project at all. |
| Is there already a bag/inventory UI open-close flow? | **No dedicated flow.** The closest existing hook is `UWTBRInteractionComponent::OnCorpseLootInteractRequested` (`WTBRInteractionComponent.h:39`), a `BlueprintAssignable` delegate broadcast from `RequestCorpseLootInteract()` (`WTBRInteractionComponent.cpp:139`) when the player presses Interact while focused on a valid corpse/container. Its doc comment literally says "Bind in Blueprint HUD/UI to open the loot widget" — i.e. this is the intended future wiring point, but nothing currently binds to it. There is no toggle/close counterpart (no "close bag" signal anywhere). |
| Is there an existing input action for bag open/close? | **No.** Grepped for `IA_Bag`, `BagAction`, `OpenBagAction`, `ToggleBagAction`, `InventoryAction`, `IA_Inventory` — zero matches. `AWTBRCharacter.h` declares `FireMainAction`, `FireSubAction`, `DodgeAction`, `SwitchTriggerAction`, `SwitchMainAction`, `SwitchSubAction`, `InteractAction` only. There is no bag-specific input action anywhere in the codebase. |
| Is there an existing interaction/cancel action flow? | **Interact: yes** (`InteractAction` → `AWTBRCharacter::Interact()` → `InteractionComponent->RequestContextInteract()`, `WTBRCharacter.cpp:487`), server-authoritative pickup RPCs already wired per the corpse-loot/dropped-trigger/ground-item priority chain. **Cancel: partial.** `AWTBRCharacter::CancelCurrentAction()` / `Server_CancelCurrentAction()` exist (`WTBRCharacter.h:266,269`) for canceling an in-progress trigger action (e.g. a charge), but there is no input binding found for it and no relationship to a bag/loot-UI cancel (Escape-to-close-bag does not exist). |

Conclusion: **the runtime UI spawn layer does not exist yet.** Everything up through the native widget parent classes and ViewModels is in place; the piece connecting "player presses a key" → "widget gets created and added to viewport" has not been built for either HUD or Bag/Loot.

---

## 4. Recommended runtime integration path

| Option | Description | Verdict |
|---|---|---|
| **A — Spawn widgets from PlayerController** | Standard UE UMG pattern: `APlayerController::BeginPlay()` (or `OnPossess`) creates the HUD widget once and adds it to viewport; owns show/hide of the Bag/Loot layer. | Not directly available — **no custom `APlayerController` subclass exists in this project.** Would require creating one first. |
| **B — Spawn widgets from custom HUD class** | `AHUD::BeginPlay()` creates and owns the widgets. | Not directly available — **no custom `AHUD` subclass exists**, and `HUDClass` is not overridden in `AWTBRGameMode`. Would require creating one first. |
| **C — Spawn widgets from Character** | `AWTBRCharacter::BeginPlay()` (guarded by `IsLocallyControlled()`) creates and owns the widgets. | **Technically simplest given current code** — `AWTBRCharacter` already owns both ViewModel components, already has an `IsLocallyControlled()` guard pattern in use elsewhere (e.g. B7D validation, `AddDefaultMappingContext`), and already exposes `GetHUDViewModelComponent()`/`GetBagLootViewModelComponent()`. Not the textbook UE pattern (HUD widgets are conventionally owned by the controller, which survives pawn death/respawn — the Character does not), but it requires **zero new classes**. |

**Recommendation: Option C for the first PIE smoke test, Option A once BR respawn/death-pawn-swap is implemented.**

Reasoning: no `APlayerController` or `AHUD` subclass currently exists, so Options A/B both require creating a new class before any widget can spawn — that is new-class work, not just "wire up the existing pattern." Option C reuses `AWTBRCharacter::BeginPlay()`, which already has the `IsLocallyControlled()` guard and direct access to both ViewModel components, so it is the smallest change that gets a widget on screen for this first test pass. However, Character-owned UI does not survive pawn destruction (BR downed/respawn will destroy and recreate the widget), so this is explicitly a **first-smoke-test-only** recommendation — flag Option A as the target once a custom `APlayerController` is introduced for the BR respawn flow.

This is a recommendation for the *next* pass, not something implemented now (see §8 and Task 3 constraints).

---

## 5. First PIE smoke test checklist

**HUD:**
- [ ] HUD appears on screen.
- [ ] HP text updates.
- [ ] HP bar updates.
- [ ] Vael text updates.
- [ ] Vael bar updates.
- [ ] Main trigger name updates.
- [ ] Main trigger cost updates.
- [ ] Main slot indicator updates.
- [ ] Sub trigger name updates.
- [ ] Sub trigger cost updates.
- [ ] Sub slot indicator updates.
- [ ] Pickup prompt appears when focusing a valid target.

**Bag/Loot:**
- [ ] Bag/Loot layer can be shown.
- [ ] Bag/Loot layer starts hidden on spawn (not visible before any interact).
- [ ] Focusing a corpse/container and pressing Interact shows the Bag/Loot layer (via `OnCorpseLootInteractRequested` → `ShowBagLootLayer()`).
- [ ] Bag capacity text updates.
- [ ] Bag slots show item names/counts/icons.
- [ ] Empty slots clear correctly.
- [ ] Corpse loot title/subtitle updates.
- [ ] Corpse loot rows show item names/types/actions/icons.
- [ ] Empty loot rows clear correctly.
- [ ] `RequestUseBagItem` calls the request-only wrapper (does not mutate locally).
- [ ] `RequestPickupCorpseLootEntry` calls the request-only wrapper (does not mutate locally).
- [ ] Client UI does not mutate inventory directly.
- [ ] Server validates the loot transfer (rejects invalid slot/entry, does not double-grant).
- [ ] **Known gap (Phase 6B):** there is currently no player-facing way to close the Bag/Loot layer once open — no input is bound to `HideBagLootLayer()`/`CloseAnyOpenLocalUI()`/`ToggleBagLootLayer()`. For this smoke test, closing it requires a manual call (e.g. via a Blueprint debug node, console, or a temporary keybind added directly in the Character Blueprint) — do not treat "can't close it" as a regression.

---

## 6. Known missing / expected placeholders

- Trigger cooldown percent is still placeholder (no cooldown-percent snapshot source yet).
- Zone number / phase / timer have no zone system source yet — HUD renders "-" per `WTBRGeneratedHUDWidget.h` comments.
- Alive/Kill/KillFeed/DamageFeedback have no snapshot source yet — widgets are bound (`BindWidgetOptional`) but not populated by `ApplySnapshot`.
- Bag weight/volume has no data source and displays "-" (no weight/volume system in `UWTBRInventoryComponent`).
- Reserved middle Bag section for Active/Reserve Trigger management is intentionally deferred — no bound widgets for it, none should be invented.
- Take All / Move / Drop / Equip-from-bag do not exist because no server-authoritative RPC exists for any of them yet.

---

## 7. Risk list

- Importing the JSON after assigning Parent Class may overwrite the WBP's Class Settings — re-verify Parent Class after every JSON import.
- "Save All" may accidentally save maps/external actors — save only the specific WBP asset.
- If generated widget names change between JSON regenerations, `BindWidgetOptional` avoids a hard compile failure, but the renamed widget will silently stop receiving updates — check the log for missing-bind warnings after any regeneration.
- If the future widget-spawn flow uses the wrong owning player/pawn, `ResolveViewModel()` (both widget classes) will fail to resolve a ViewModel and every field stays at its default/placeholder text.
- Corpse loot focus refresh is not event-driven: `UWTBRInteractionComponent` has no "focused interactable changed" delegate (only `OnCorpseLootInteractRequested`, which fires on interact-key-press, not on focus change — see `WTBRBagLootViewModelComponent.h:106-118`). `RefreshBagLootSnapshot()` re-resolves the focused container each time it runs, but if focus changes to a different container between refreshes with nothing else triggering a refresh, the snapshot goes stale until the next trigger (`OnInventoryChanged` or the bound container's `OnCorpseLootEntriesChanged`). A future widget should call `RefreshFromViewModel()` manually on relevant input events (e.g. when the pickup prompt becomes visible) until a real focus-changed delegate exists.
- No input action for bag open/close exists yet — assigning one in the Enhanced Input mapping context is a prerequisite for any Bag/Loot PIE test, not just a UMG step.

---

## 8. Next implementation candidates (code-only, after this plan)

- Add runtime widget spawn code only after the human confirms the desired owner (Character now vs. PlayerController later per §4).
- Add a UI open/close controller only after the bag-open/cancel input action mapping is confirmed with the human.
- Add a focus-change delegate on `UWTBRInteractionComponent` later, so corpse loot can refresh reactively instead of via the re-resolve-every-refresh compromise in §7.
- Add the Active/Reserve Trigger middle Bag section later (deferred, no design lock yet).
- Add Take All / Move / Drop / Equip-from-bag only after their server RPCs are designed (currently no design lock, no RPC).

---

## 9. Phase 6A — Runtime UI spawn scaffold (implemented)

Minimal C++ scaffold added to `AWTBRCharacter` so a locally controlled character can create
the generated HUD and Bag/Loot widgets at runtime, without touching any WBP asset. No JSON
import, no `.uasset`/`.umap` edit, no plugin-source change.

**Widget class properties added** (`Source/WTBR/WTBRCharacter.h`, `EditDefaultsOnly, BlueprintReadOnly, Category="WTBR | UI"`):
- `TSubclassOf<UUserWidget> HUDWidgetClass` — target for future assignment: `/Game/UI/Generated/WBP_HUD_Generated`.
- `TSubclassOf<UUserWidget> BagLootWidgetClass` — target for future assignment: `/Game/UI/Generated/WBP_BagLootLayer_Generated`.

Both are plain `TSubclassOf<UUserWidget>` editable references, not hardcoded asset paths — the human assigns the actual WBP class on the Character Blueprint/CDO once each WBP's Parent Class is set (§2, steps 8-10). No soft-path fallback was added: if left unassigned, `CreateLocalPlayerUI()` simply skips that widget (see below), which is safer than guessing a hardcoded path to an asset that may not exist yet or may not have its parent class assigned.

**Runtime instances added** (`Transient, BlueprintReadOnly, Category="WTBR | UI"`):
- `TObjectPtr<UUserWidget> HUDWidgetInstance`
- `TObjectPtr<UUserWidget> BagLootWidgetInstance`

Typed as plain `UUserWidget*` rather than the native parent types (`UWTBRGeneratedHUDWidget*` / `UWTBRBagLootWidget*`) since the human has not yet assigned/compiled the WBP parent classes — `CreateWidget<UUserWidget>(PC, *WidgetClass)` is safe regardless of what the assigned class actually is, and nothing in this pass needs to call HUD/Bag-Loot-specific members on these pointers.

**Lifecycle functions added** (all `Category="WTBR | UI"`, `Source/WTBR/WTBRCharacter.h/.cpp`):
- `CreateLocalPlayerUI()` — locally-controlled only (`IsLocallyControlled()` + valid `APlayerController`). Creates `HUDWidgetInstance` via `CreateWidget<UUserWidget>(PC, HUDWidgetClass)` and `AddToViewport(0)` if `HUDWidgetClass` is set and the instance doesn't already exist; same for `BagLootWidgetInstance` with `AddToViewport(10)` (higher Z-order, draws above HUD) followed by `SetVisibility(ESlateVisibility::Collapsed)` so Bag/Loot starts hidden. Guarded by `!IsValid(...Instance)` before each creation, so repeat calls are idempotent — no duplicate widgets.
- `DestroyLocalPlayerUI()` — `RemoveFromParent()` on both instances (if valid) then nulls both pointers. Safe to call multiple times, including when nothing was ever created.
- `ShowBagLootLayer()` — sets `BagLootWidgetInstance` visibility to `Visible`. If the instance doesn't exist yet, calls `CreateLocalPlayerUI()` first; if it's still invalid afterward (e.g. `BagLootWidgetClass` unset, or not locally controlled), logs a `WTBR_VALIDATION_LOG(Warning, ...)` and returns — no gameplay mutation either way.
- `HideBagLootLayer()` — sets visibility to `Collapsed` if the instance is valid; no-op otherwise.
- `ToggleBagLootLayer()` — flips between the two based on `IsBagLootLayerVisible()`.
- `IsBagLootLayerVisible() const` — `true` only if the instance is valid, in viewport, and not `Collapsed`.

None of these functions touch inventory, loot entries, trigger slots, HP, or Vael — purely presentation (`AddToViewport`/`RemoveFromParent`/`SetVisibility`), matching the existing hard rule that this UI layer is read-only/request-only.

**Lifecycle hooks:**
- `BeginPlay()`: after the existing delegate bindings and `RefreshHUDHints()`, added a single `CreateLocalPlayerUI();` call. No new timer/retry was added — `AddDefaultMappingContext()` (already called earlier in the same `BeginPlay()`) relies on `IsLocallyControlled()` being reliable at this point in this codebase, so `CreateLocalPlayerUI()` follows the same assumption rather than adding new polling machinery (kept minimal per Task 5).
- `EndPlay(EEndPlayReason::Type)`: new override added (character previously had none) that calls `DestroyLocalPlayerUI()` before `Super::EndPlay()`.

**Interaction hook (Task 6) — implemented, not deferred:** `AWTBRCharacter::BeginPlay()` now also binds `InteractionComponent->OnCorpseLootInteractRequested.AddDynamic(this, &AWTBRCharacter::OnCorpseLootInteractRequestedHandler)`, mirroring the existing pattern already used for `HealthComponent->OnLimbDestroyed`, `StaminaComponent->OnStaminaChanged`, etc. (same component-owned-by-Character lifetime, same binding style). The handler calls `ShowBagLootLayer()` only — it does not call `RequestPickupCorpseLootEntry` or touch any gameplay state. This was judged safe because `InteractionComponent` is a component directly owned by this `AWTBRCharacter` (not a separately-lifetimed object), and the delegate only ever broadcasts on the interacting client in response to an explicit Interact key press.

**Character-owned UI is temporary:** accepted only for this first PIE smoke test because no custom `APlayerController`/`AHUD` subclass exists yet (Phase 5A finding, §4). Both the header comment above the new properties and the implementation comment above the function block say so explicitly. Character-owned widgets do not survive pawn destruction, so this must migrate to a `PlayerController`/`AHUD`/UI-manager owner once BR respawn/pawn-swap is implemented — tracked as a Phase 6B+ candidate, not done here.

**Manual Unreal setup still required** (see updated §2): assign the actual WBP class to `HUDWidgetClass`/`BagLootWidgetClass` on the Character Blueprint (or CDO) — this cannot be done from C++, and must happen *after* each WBP's Parent Class is set and compiled, or the widget will be created from the wrong/base `UUserWidget` class with none of the generated bindings.

**Build:** Development Editor Win64 — 12/12 actions, 0 errors, 0 warnings (via `E:\UE_5.1\Engine\Build\BatchFiles\Build.bat WTBREditor Win64 Development`).

**Known limitations:**
- No input action exists yet to call `ToggleBagLootLayer()`/`ShowBagLootLayer()`/`HideBagLootLayer()` directly — per this pass's constraints, no Bag input action was added. The only way to trigger `ShowBagLootLayer()` right now is the corpse-loot interact hook above; there is still no player-facing way to close it again (no Cancel/close binding wired to `HideBagLootLayer()`).
- `HUDWidgetClass`/`BagLootWidgetClass` default to unset (`nullptr`) until a human assigns them on the Character Blueprint/CDO — until then, `CreateLocalPlayerUI()` runs every `BeginPlay()` and creates nothing (Phase 6C added a log line for this state — see §11 — but it is still a safe no-op, not a blocker).
- Z-order values (`0` for HUD, `10` for Bag/Loot) are hardcoded smoke-test constants, not exposed as tunable properties — acceptable for a first pass, revisit if more UI layers are added later.
- Character-owned UI (see above) is a known-temporary design, not a final architecture.

---

## 10. Phase 6B — Bag/Loot open-close input flow audit + safe close hook

No `.uasset`/`.umap`/plugin-source changes. No Enhanced Input assets created or edited. No
Take All / Move / Drop / Equip-from-bag added.

**Input/cancel audit findings:**

| Question | Finding |
|---|---|
| Is there an existing Cancel input action? | **No.** Grepped `AWTBRCharacter.h`/`.cpp` for `CancelAction` — zero matches. There is no `UInputAction* CancelAction` property and no `EIC->BindAction(...Cancel...)` call anywhere in `SetupPlayerInputComponent()`. `AWTBRCharacter::CancelCurrentAction()` / `Server_CancelCurrentAction()` exist and work (they cancel an in-progress trigger action, e.g. a charge — see `WTBRCharacter.cpp:515,1873`), but nothing currently calls `CancelCurrentAction()` from player input. It is only reachable via Blueprint (`BlueprintCallable`) or via `UWTBRHUDViewModelComponent::RequestCancelCurrentUIOrAction()` (`WTBRHUDViewModelComponent.cpp:373`), a request-only wrapper intended for a future HUD Cancel button — that wrapper calls `Character->CancelCurrentAction()` only, so it is scoped to trigger-action-cancel, not UI-close. |
| Is there an existing Interact action that could close UI if the panel is open? | **Interact exists** (`InteractAction` → `AWTBRCharacter::Interact()` → `InteractionComponent->RequestContextInteract()`), but its behavior is a fixed priority chain (corpse/container → dropped trigger → BR ground item → generic interactable → no-op) with real gameplay side effects (server pickup RPCs) at priorities 2-3. Making Interact double as "close if a panel is open" would either change gameplay-interact behavior or require a separate branch with no existing precedent — not "clearly safe" per this pass's constraint, so **left unchanged**. |
| Is there an existing input mapping object where bag input should eventually be added? | `DefaultMappingContext` (`TObjectPtr<UInputMappingContext>`, `WTBRCharacter.h:75`) is the single existing IMC the Character already applies via `AddDefaultMappingContext()`. This is the natural future home for an `IA_Bag`/`IA_Cancel` mapping, but no such input action exists in it today (audit only — the IMC asset itself was not opened or edited). |
| Is there any safe C++ hook for closing Bag/Loot now without editing input assets? | **Yes** — `AWTBRCharacter::CloseAnyOpenLocalUI()` (new, this pass). It is a plain `BlueprintCallable`/`UFUNCTION`, reachable from Blueprint (e.g. a temporary debug key added directly in the Character Blueprint's input graph, or a future WBP Cancel button) without touching any `.uasset` input asset. |

**Close/toggle helpers added** (`Source/WTBR/WTBRCharacter.h/.cpp`, `Category="WTBR | UI"`):
- `CloseAnyOpenLocalUI()` — closes the Bag/Loot layer if `IsBagLootLayerVisible()` is true, via the existing `HideBagLootLayer()`. Written as a single entry point (rather than every future caller calling `HideBagLootLayer()` directly) so a later second local panel (e.g. the still-unimplemented Q/E hold trigger-selection wheel) only needs to be added inside this one function.
- `IsAnyLocalUIPanelOpen() const` — currently a thin wrapper over `IsBagLootLayerVisible()`.

Both are safe on non-locally-controlled/server characters: `IsBagLootLayerVisible()` requires `BagLootWidgetInstance` to be valid and in-viewport, which is never true on a character that never ran `CreateLocalPlayerUI()` successfully (guarded by `IsLocallyControlled()`). Neither function mutates inventory, loot, trigger slots, or any other gameplay state — no gameplay is destroyed or requested.

**Input hook: deferred, not implemented.** Per the audit above, no existing Cancel input action exists in C++ to hook — inventing one would mean adding a new `UInputAction*` property and, eventually, an IMC entry, which crosses into "new Enhanced Input asset" territory this pass must not touch. No binding code was added. Manual future step (documented, not implemented):

> Create/assign `IA_Bag` or `IA_Cancel` in Enhanced Input (in `DefaultMappingContext` or a dedicated UI mapping context) and bind it in `AWTBRCharacter::SetupPlayerInputComponent()` to `ToggleBagLootLayer()` / `CloseAnyOpenLocalUI()`, following the same optional-binding pattern already used for `InteractAction` (`if (IsValid(...Action)) { EIC->BindAction(...); }` — defaults null, skipped if unassigned, remappable, no key hardcoded).

**Current way to open Bag/Loot:** unchanged from Phase 6A — focus a corpse/container and press Interact; `OnCorpseLootInteractRequested` fires and the bound handler calls `ShowBagLootLayer()`.

**Current way to close Bag/Loot:** none exposed to player input. `CloseAnyOpenLocalUI()`/`HideBagLootLayer()`/`ToggleBagLootLayer()` exist and work if called (e.g. from a Blueprint debug node), but no key/action reaches them yet. This is the same gap noted at the top of this pass, now narrowed to "needs one IMC action + one bind call," not "needs new C++ scaffolding."

**Interaction hook safety review (Task 4) — reviewed, no changes needed:**
- `ShowBagLootLayer()` only creates/shows UI for locally controlled characters — confirmed via `CreateLocalPlayerUI()`'s `IsLocallyControlled()` guard, which every path (`ShowBagLootLayer()`'s auto-create-if-missing branch included) goes through before touching a widget.
- No widget is created on a dedicated server or remote simulated proxies — `CreateLocalPlayerUI()` returns immediately when `!IsLocallyControlled()`, which is always true for server-side and simulated-proxy character instances.
- No duplicate-binding risk — `InteractionComponent->OnCorpseLootInteractRequested.AddDynamic(...)` runs once in `BeginPlay()`, and `BeginPlay()` is a one-time actor lifecycle event; this matches the existing unguarded `AddDynamic` pattern already used for `HealthComponent->OnLimbDestroyed`, `StaminaComponent->OnStaminaChanged`, and `TriggerSetComponent`'s two delegates in the same function.
- `EndPlay()` does not explicitly unbind the delegate — left as-is, matching the existing precedent (none of the other Character delegate bindings above are unbound in `EndPlay()` either) — safe because `InteractionComponent` is owned by and destroyed together with this `AWTBRCharacter`, so there is no dangling-reference window.
- Confirmed the delegate can only ever fire from this character's own local input: `RequestContextInteract()`/`RequestCorpseLootInteract()` are also called directly from several automation test files (`WTBRCorpseLootAutomationTests.cpp`, `WTBRDroppedTriggerInteractAutomationTests.cpp`, `WTBRGroundItemPickupAutomationTests.cpp`) and from `UWTBRHUDViewModelComponent::RequestPickupFocusedTarget()` (itself `IsLocallyControlled()`-gated) — in every case, if `BagLootWidgetClass` is unset (current state, pre-human-setup) or the character is not locally controlled, `ShowBagLootLayer()`'s widget-creation attempt silently no-ops (logs a `WTBR_VALIDATION_LOG(Warning, ...)` only). No changes were needed; no fix applied.
- Verified with the full existing automation suite after these changes: `Automation RunTests WTBR;Quit` → **78/78 PASS, 0 fail** (`Source/Phase6B_AutoTest.log`), confirming the Phase 6A delegate binding and this pass's additions did not regress `WTBR.CorpseLoot`, `WTBR.Inventory`, `WTBR.DroppedTrigger`, `WTBR.GroundItemPickup`, or `WTBR.UI.Contract`.

**Manual Unreal/input setup still required:** everything from §2 (Parent Class assignment + `HUDWidgetClass`/`BagLootWidgetClass` assignment), plus the new `IA_Bag`/`IA_Cancel` Enhanced Input step documented above — none of which was done in this pass.

**Build:** C++ changed (two small additive functions) — Development Editor Win64 rebuilt via `E:\UE_5.1\Engine\Build\BatchFiles\Build.bat WTBREditor Win64 Development` → 0 errors, 0 warnings.

---

## 11. Phase 6C — Runtime UI readiness guard + debug notes

No `.uasset`/`.umap`/plugin-source changes. No new PlayerController/HUD/UI-manager class,
no input assets, no input bindings, no inventory mutation, no hardcoded WBP asset paths.

**Inspection findings (Task 1):** reviewed `CreateLocalPlayerUI()`, `DestroyLocalPlayerUI()`,
`ShowBagLootLayer()`, `HideBagLootLayer()`, `ToggleBagLootLayer()`, `CloseAnyOpenLocalUI()`.
No missing null guards and no repeated-`AddToViewport` risk were found — every creation path
was already gated by `!IsValid(...Instance)` before creating + adding to viewport (Phase 6A
got this right originally), and every visibility/close path already null-checks its instance
first. The one real gap was **diagnostics**: none of these functions logged anything, so a
human hitting "nothing appeared" during the first PIE test would have no Output Log signal to
tell "not locally controlled" apart from "class unassigned" apart from "worked, just still
Collapsed."

**Diagnostics added** (all via the existing `WTBR_VALIDATION_LOG` macro — gated behind the
`wtbr.Debug.ValidationLogs` console variable, default `0`/off, so nothing is logged unless a
human explicitly turns it on; no per-tick logging was added anywhere):
- `CreateLocalPlayerUI()`: logs (Verbose) when skipped because not locally controlled; logs (Warning) if locally controlled but `Controller` isn't a valid `APlayerController` yet; calls the new `ValidateLocalUISetup(true)` up front so any unassigned widget class is called out before creation proceeds; logs (Log) on successful HUD widget creation and on successful Bag/Loot widget creation (noting it starts hidden).
- `ShowBagLootLayer()`: the single generic warning from Phase 6B was split into four distinct cases — success (Verbose), skipped-not-locally-controlled (Verbose), `BagLootWidgetClass` unassigned (Warning), and an "unexpected" fallback (Warning) for the case where the instance is still invalid despite passing the other two checks.
- `ToggleBagLootLayer()`: now short-circuits with a Warning log when both `BagLootWidgetInstance` and `BagLootWidgetClass` are unset, instead of silently falling through into `ShowBagLootLayer()` (which would have logged its own warning anyway, but this makes the "toggle called in a hopeless state" case explicit at the call site that started it).

**Validation helper added (Task 3):** `ValidateLocalUISetup(bool bLogIfMissing = true) const` — returns `true` only if both `HUDWidgetClass` and `BagLootWidgetClass` are assigned. Never a gameplay blocker and never crashes on missing setup — it is a pure read of the two `TSubclassOf` properties, with an optional (default-on) logging side effect. `CreateLocalPlayerUI()` now calls it once at the top of every invocation.

**How to interpret common log messages** (all prefixed `[UI]`, all require `wtbr.Debug.ValidationLogs 1` in console to appear):
| Log line (paraphrased) | Meaning |
|---|---|
| `ValidateLocalUISetup: HUDWidgetClass is not assigned yet` | Human has not yet assigned `HUDWidgetClass` on the Character Blueprint/CDO. HUD will never appear until this is set. |
| `ValidateLocalUISetup: BagLootWidgetClass is not assigned yet` | Same, for Bag/Loot. |
| `CreateLocalPlayerUI: skipped (not locally controlled)` | Expected on the server and on every remote simulated proxy — not an error. Only the true local player's own pawn should proceed past this. |
| `CreateLocalPlayerUI: skipped (locally controlled but no valid PlayerController yet)` | Possession/controller-assignment timing edge case — should be rare given `BeginPlay` ordering already relied on elsewhere in this class (e.g. `AddDefaultMappingContext`); if seen often, flag for investigation. |
| `CreateLocalPlayerUI: HUD widget created and added to viewport` | HUD widget instance exists and is on screen (whatever it visually renders depends on the WBP's actual content/parent-class binding). |
| `CreateLocalPlayerUI: BagLoot widget created, added to viewport, hidden initially` | Bag/Loot widget instance exists, in viewport, but `Collapsed` until `ShowBagLootLayer()`/`ToggleBagLootLayer()` is called. |
| `ShowBagLootLayer: Bag/Loot layer now visible` | Visibility was successfully set to `Visible`. |
| `ShowBagLootLayer: BagLootWidgetClass is not assigned — nothing to show` | Human needs to assign `BagLootWidgetClass`. |
| `ToggleBagLootLayer: no BagLootWidgetInstance and no BagLootWidgetClass assigned` | Nothing to toggle at all yet — same root cause as above. |

**Troubleshooting checklist (added to this doc for the next PIE session):**

*If HUD does not appear:*
- Is `WBP_HUD_Generated`'s Parent Class set to `WTBRGeneratedHUDWidget`?
- Was the WBP compiled and saved after that?
- Is `HUDWidgetClass` assigned on the Character Blueprint/CDO?
- Is this the locally controlled player (not a spectator/remote proxy)?
- Does the Output Log (with `wtbr.Debug.ValidationLogs 1`) show a missing-`HUDWidgetClass` warning?

*If Bag/Loot does not appear:*
- Is `WBP_BagLootLayer_Generated`'s Parent Class set to `WTBRBagLootWidget`?
- Was the WBP compiled and saved after that?
- Is `BagLootWidgetClass` assigned on the Character Blueprint/CDO?
- Was corpse-loot interaction actually triggered (focused a valid container, pressed Interact)? Remember there is still no other way to open it (§10).
- Does the Output Log show a missing-`BagLootWidgetClass` warning?

*If values do not update (widgets appear but stay blank/placeholder):*
- Does the character have the expected ViewModel component (`HUDViewModelComponent`/`BagLootViewModelComponent`)?
- Is the WBP's Parent Class actually assigned correctly (not left as base `UserWidget`)?
- Were the generated widget names preserved (not renamed) after the last JSON import?
- Do the `BindWidgetOptional` names in the native parent class still match the names in the generated JSON (see §7 risk: renames silently stop updating, no compile error)?

**Reminders (unchanged from Phase 6A/6B, restated for this pass):** Character-owned UI is a temporary smoke-test scaffold — it should migrate to a `PlayerController`/`AHUD`/UI-manager owner once BR respawn/pawn-swap exists. No new UI systems, input assets, or gameplay mutation were added in this pass.

**Build:** C++ changed (diagnostics + one small validation helper, no behavior change to when/whether widgets are created or shown) — Development Editor Win64 rebuilt via `E:\UE_5.1\Engine\Build\BatchFiles\Build.bat WTBREditor Win64 Development` → 0 errors, 0 warnings. Re-ran `Automation RunTests WTBR;Quit` → **78/78 PASS, 0 fail** (`Source/Phase6C_AutoTest.log`), confirming no regression.
