# WTBR UMG HUD Widget Breakdown / Conversion Plan

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1  
Baseline: `fae15d6` - Docs: Add approved HUD HTML CSS prototype

## Purpose / Status

This document maps the approved in-match HUD HTML/CSS prototype into a future UMG widget structure.

Status:

- Docs-only conversion plan.
- No WBP, UMG, Blueprint, `.uasset`, `.umap`, or binary asset implementation is approved by this document.
- No C++ or automation test changes are approved by this document.
- The HTML/CSS prototype is a visual reference only.
- The HTML/CSS prototype does not approve a production Web Browser Widget in-match HUD.
- Final implementation must follow `Docs/WTBR_UI_CONTRACT_PLAN.md` and the server-authoritative request-only rules.

## Approved HUD Layout Mapping

### HUD Root

Future widget:

- `WBP_WTBRHUDRoot`

Likely role:

- Owns the root Canvas Panel / Overlay.
- Applies safe-zone and DPI scaling rules.
- Groups visibility layers for persistent HUD, contextual prompts, combat feedback, and temporary feeds.
- Does not mutate gameplay state.

Suggested children:

- `WBP_MatchSummaryPanel`
- `WBP_KillFeedPanel`
- `WBP_MinimapPanel`
- `WBP_ZonePhaseStrip`
- `WBP_VitalsPanel`
- `WBP_QuickItemChip`
- `WBP_TriggerStack`
- `WBP_CancelPrompt`
- `WBP_Crosshair`
- `WBP_PickupPrompt`
- Future contextual feedback widgets.

### Top-Left

Future widgets:

- `WBP_MatchSummaryPanel`
- `WBP_KillFeedPanel`

`WBP_MatchSummaryPanel`:

- Always visible during an active match unless the entire HUD is hidden.
- Shows ALIVE count.
- Shows player KILL count.
- Should remain compact and readable.
- Should not contain kill feed entries in the same panel background.

`WBP_KillFeedPanel`:

- Separate from match summary.
- Hidden by default.
- Visible only on elimination/death events.
- Fades or hides after a short duration.
- Uses compact, lighter/translucent entries that do not dominate the top-left.

### Top-Center

Future widget:

- None by default.

Rules:

- Reserved / protected visibility corridor.
- No default top-center panel.
- Temporary future messaging must be human-approved and should avoid blocking combat readability.

### Top-Right

Future widgets:

- `WBP_MinimapPanel`
- `WBP_ZonePhaseStrip`

`WBP_MinimapPanel`:

- Anchored top-right.
- Shows the tactical minimap and player orientation.
- Keeps safe margins from screen edges.

`WBP_ZonePhaseStrip`:

- Placed under the minimap.
- Shows ZONE / TIMER / PHASE.
- Remains compact and visually subordinate to the minimap.

### Bottom-Left

Future widgets:

- `WBP_VitalsPanel`
- `WBP_QuickItemChip`

`WBP_VitalsPanel`:

- Shows HP current/max.
- Shows VAEL current/max or percent.
- Uses readable bars and values.
- Does not request or perform direct HP/VAEL mutation.

`WBP_QuickItemChip`:

- Badge-style quick item display.
- Shows item icon as the main element.
- Shows attached quantity badge such as `x3`.
- Shows current QuickItem action binding below the icon.
- Does not show item name in the compact HUD chip.
- Supports normal, warning, and empty states.
- Tap uses the displayed item through an authoritative request path.
- Hold opens the Quick Item Dialog / Radial.

### Right Side

Future widgets:

- `WBP_TriggerStack`
- `WBP_TriggerCard_Main`
- `WBP_TriggerCard_Sub`

Alternative:

- Reusable `WBP_TriggerCard` with a Main/Sub style parameter.

Rules:

- Cards remain stacked on the right side.
- Main identity uses cyan/blue language.
- Sub identity uses orange/red language.
- Active slot diamond indicator is centered above the MAIN/SUB label.
- Cards use translucent/glass styling, not heavy solid black panels.
- Trigger use remains routed through existing action flow, not direct UI mutation.

### Above Right-Side Cards

Future widget:

- `WBP_CancelPrompt`

Rules:

- Hidden by default.
- Visible only during cancelable states.
- Displays current Cancel action binding and label, for example `[CancelActionKey] Cancel`.
- Cancel remains presentation-only unless the active flow defines a specific server-authoritative cancel request.

### Center

Future widgets:

- `WBP_Crosshair`
- `WBP_PickupPrompt`
- Future optional contextual feedback widgets.

`WBP_Crosshair`:

- Subtle and always readable.
- Should not compete with prompt or combat target visibility.

`WBP_PickupPrompt`:

- Hidden by default.
- Visible only when a valid pickup/interact target exists.
- Displays current pickup/interact action binding and text, for example `[PickupActionKey] Pick Up`.
- Requests context interact / pickup only; it must not destroy, hide, or consume target actors locally.

Future optional feedback:

- Hit marker.
- Damage feedback.
- Ping / enemy / focus marker.

Rules:

- Hidden by default.
- Event-driven or contextual only.
- Must not be visible in the default clean center HUD.

### Bottom-Right

Future widget:

- None by default.

Rules:

- Reserved / clear by default.
- Avoid permanent bottom-right panels unless separately approved.

## UMG Layout Guidance

### Root Layout

Recommended containers:

- Canvas Panel for screen-region anchoring.
- Overlay for layered full-screen HUD content.
- Safe Zone for platform/display-safe margins when needed.
- Scale Box or DPI rules only after confirming project DPI strategy.

Notes:

- Keep top-left, top-right, bottom-left, right-side, and center widgets as separate anchored groups.
- Avoid nesting every screen region inside a single large decorative panel.
- Use visibility states rather than removing widgets when event-driven widgets need animation or fade-out.

### Match Summary

Recommended containers:

- Border or Image for translucent panel background.
- HorizontalBox for ALIVE / KILL groups.
- TextBlock for labels and values.
- Optional Image or TextBlock for small stat icon/glyph.
- SizeBox for stable compact dimensions.

### Kill Feed

Recommended containers:

- VerticalBox for stacked entries.
- Border or Image per feed entry.
- HorizontalBox or Grid Panel per entry for killer / verb / victim.
- Widget animation for fade-in/fade-out.

State:

- Hidden by default.
- Visible during recent elimination/death events.
- Fade out after short duration.

### Minimap / Zone Strip

Recommended containers:

- Border/Image for minimap frame.
- Image/Material for minimap render or placeholder.
- Overlay for markers, player arrow, and compass.
- HorizontalBox or Grid Panel for ZONE / TIMER / PHASE strip.

### Vitals

Recommended containers:

- VerticalBox for HP row and VAEL row.
- Border/Image for glass panel.
- HorizontalBox for icon + value/bar.
- ProgressBar or custom bar material for HP/VAEL bars.
- TextBlock for current/max and percent values.

### Quick Item Chip

Recommended containers:

- VerticalBox for icon badge and key binding display.
- Overlay for icon plus attached count badge.
- Image for item icon.
- Border/TextBlock for quantity badge.
- Reusable input glyph widget for the QuickItem action binding.

State:

- Normal.
- Warning.
- Empty / disabled.

### Trigger Cards

Recommended containers:

- VerticalBox for slot indicator and content.
- Border/Image brush for translucent card background.
- HorizontalBox or Grid Panel for icon, label/name, and binding glyph.
- TextBlock for MAIN/SUB label and trigger name.
- Image for trigger icon.
- TextBlock or small widget for active slot diamond indicator.

### Cancel Prompt

Recommended containers:

- HorizontalBox inside Border.
- Reusable input glyph widget.
- TextBlock for `Cancel`.

State:

- Hidden by default.
- Visible only during cancelable states.

### Center Combat / Pickup

Recommended containers:

- Overlay for crosshair and contextual center-lower prompt.
- Image or lightweight custom widget for crosshair.
- HorizontalBox inside Border for pickup prompt.
- Reusable input glyph widget for Pickup/Interact action.

State:

- Pickup hidden by default.
- Pickup visible only with valid target.
- Hit/damage/ping/focus feedback hidden by default and event-driven.

## Style Translation From CSS To UMG

CSS variables:

- Translate to UMG style constants, a style data asset, or a native style struct.
- Keep color, opacity, spacing, radius, and glow intensity centralized.

Translucent glass panel:

- Use Border/Image brush with semi-transparent fill.
- Consider a lightweight material only if blur/refraction is necessary and approved.
- Keep opacity readable in both bright and dark environments.

Glow:

- Use low-intensity material, image border, or shadow-like brush treatment.
- Low glow by default.
- Stronger glow only for active, warning, critical, or interactable states.

Angular frame / CSS clip-path:

- Prefer border image, material mask, or 9-slice-like image treatment if feasible.
- Do not require complex clipping if it harms readability or implementation cost.

Progress bars:

- Use UMG ProgressBar for simple HP/VAEL bars.
- Use a custom material only if the final visual target requires it.

Key binding chip:

- Use a reusable input glyph/display-name widget.
- It should receive an action name or resolved display value from a UI contract/viewmodel layer.
- It should not hardcode physical keys.

## Remappable Key Display

Rules:

- All visible key labels/glyphs must resolve from current Input Mapping / Enhanced Input.
- Mockup labels such as `X`, `Q`, `S`, `E`, `4`, `LMB`, and `RMB` are examples only.
- Future implementation should support keyboard, mouse, and gamepad glyph/display names.
- Do not hardcode physical keys in widget text.
- Missing or unbound actions should render a safe unbound/disabled presentation state instead of guessing a key.

Likely binding displays:

- Cancel prompt: current Cancel action binding.
- Quick Item chip: current QuickItem action binding.
- Pickup prompt: current Pickup/Context Interact action binding.
- Main Trigger card: current Main Use binding.
- Sub Trigger card: current Sub Use binding.
- Main/Sub slot indicators or selectors: current slot/select bindings if exposed.

## UI Contract Bindings

Reference: `Docs/WTBR_UI_CONTRACT_PLAN.md`

### Likely Read-Only Data Dependencies

Match / feed:

- Alive count.
- Player kill count.
- Kill feed entries.

Zone:

- Zone index.
- Zone timer.
- Phase.

Vitals:

- HP current.
- HP max.
- VAEL current.
- VAEL max or percent.
- Derived HP/VAEL warning state.

Quick Item:

- Current quick item icon.
- Current quick item count.
- Current quick item state: normal, warning, empty.
- Current QuickItem action binding display/glyph.

Triggers:

- Main trigger name.
- Main trigger icon.
- Main trigger style/readiness state.
- Main active slot index.
- Sub trigger name.
- Sub trigger icon.
- Sub trigger style/readiness state.
- Sub active slot index.
- Main/Sub action binding displays/glyphs.

Prompts:

- Cancel prompt visibility/state.
- Cancel action binding display/glyph.
- Pickup prompt visibility.
- Pickup prompt text/icon.
- Pickup/Interact action binding display/glyph.

Future contextual feedback:

- Hit marker visibility and animation state.
- Damage feedback state.
- Ping/focus marker state.

### Request-Only Actions

Quick Item Tap:

- Request use of displayed item.
- Must route to the authoritative inventory use path.

Quick Item Hold:

- Open Quick Item Dialog / Radial.
- Dialog/radial presentation state is local UI state.

Pickup Prompt:

- Request context interact / pickup.
- Must not consume, destroy, hide, or mutate pickup target locally.

Trigger Use:

- Request through existing gameplay action flow.
- UI must not directly activate or mutate trigger state.

Cancel:

- Request/cancel current cancelable local UI mode.
- Send a server request only if the active flow explicitly requires one.

## Visibility / State Rules

- Kill feed is hidden by default.
- Kill feed is event-driven and visible only for elimination/death events.
- Kill feed fades or hides after a short duration.
- Pickup prompt is hidden by default.
- Pickup prompt is visible only when a valid pickup/interact target exists.
- Cancel prompt is hidden by default.
- Cancel prompt is visible only during cancelable state.
- Hit marker, damage feedback, enemy ping, and focus marker are hidden by default.
- Hit marker, damage feedback, enemy ping, and focus marker are event-driven/contextual only.
- Quick Item supports empty, warning, and normal states.
- Low glow is default.
- Stronger glow is reserved for active, warning, critical, or interactable states.

## Non-Goals

- No WBP/UMG implementation in this pass.
- No Blueprint asset creation.
- No `.uasset`, `.umap`, or binary asset work.
- No Web Browser Widget production HUD.
- No gameplay mutation from UI.
- No server RPC changes yet.
- No C++ changes.
- No test changes.
- No staging, commit, or push in this pass.

## Future Implementation Pass Checklist

Before WBP/UMG implementation:

- Human approves exact files/assets to create or edit.
- Confirm C++ UI contract is ready, or identify missing viewmodel/delegate/request API.
- Confirm input glyph/display strategy.
- Confirm DPI/safe-zone strategy.
- Confirm no hardcoded physical key labels.
- Confirm UI sends requests only and never mutates inventory, loot, ground item, HP/VAEL, or trigger state directly.
- Confirm kill feed timing, max entry count, and fade behavior.
- Confirm pickup prompt priority with context interact.
- Confirm Quick Item empty/warning/normal state thresholds.
- Confirm Main/Sub card reusable-widget strategy.
- Confirm whether any future hit/damage/ping/focus marker is approved for default HUD visibility.

## Hard Rules To Preserve

- Gameplay remains server-authoritative.
- Client UI must not directly mutate inventory, trigger slots, loot entries, ground items, dropped triggers, HP, VAEL, or other authoritative gameplay state.
- UI sends requests only.
- Server validates requests and mutates authoritative state only on success.
- UI refreshes from replicated state or read-only viewmodel snapshots derived from replicated state.
- Do not hardcode physical keys.
- Do not use `git add .`.
- Do not stage `.claude/settings.local.json`, `.claude/scheduled_tasks.lock`, ThirdPersonMap external actor `.uasset`, or logs.
