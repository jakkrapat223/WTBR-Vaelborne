# UMG JSON Generator — Plugin README

**Engine:** Unreal Engine 5.1  
**Type:** Editor-only plugin (C++)  
**Version:** 1.0 MVP

---

## What It Does

Reads a `.json` UI layout file and generates a **Widget Blueprint** (`.uasset`) under
`/Game/UI/Generated/`. The WBP opens automatically in UMG Designer after import.

---

## Installation

1. Copy the `UMGJsonGenerator/` folder into your project's `Plugins/` directory:
   ```
   YourProject/
   └── Plugins/
       └── UMGJsonGenerator/
           ├── UMGJsonGenerator.uplugin
           └── Source/...
   ```

2. Add the plugin to your `.uproject`:
   ```json
   {
     "Name": "UMGJsonGenerator",
     "Enabled": true
   }
   ```

3. Regenerate project files (right-click `.uproject` → *Generate Visual Studio project files*).

4. Build and open in Unreal Editor.

5. Enable the plugin if prompted: **Edit → Plugins → Search "UMG JSON Generator" → Enable**.

---

## How to Import a JSON File

**Method A — Tools Menu:**
1. Open the project in Unreal Editor.
2. Click **Tools** in the top menu bar.
3. Select **Import UMG JSON...**.
4. Browse to and select your `.json` file.
5. The WBP is created at `/Game/UI/Generated/<widgetName>` and opens in UMG Designer.

**Method B — Toolbar Button:**
- A button labelled **Import UMG JSON** is added to the Level Editor toolbar.

---

## JSON Schema

```json
{
  "widgetName": "WBP_MyScreen",
  "parentClass": "/Script/WTBR.WTBRGeneratedHUDWidget",
  "resolution": { "width": 1920, "height": 1080 },
  "root": {
    "type": "CanvasPanel",
    "name": "RootCanvas"
  },
  "widgets": [
    {
      "type":      "TextBlock",
      "name":      "Txt_Title",
      "parent":    "RootCanvas",
      "text":      "HELLO WORLD",
      "position":  { "x": 760, "y": 160 },
      "size":      { "width": 400, "height": 80 },
      "fontSize":  48,
      "color":     "#FFFFFF",
      "zOrder":    1
    }
  ]
}
```

### Required Fields

| Field | Description |
|-------|-------------|
| `widgetName` | Asset name. Must be unique under `/Game/UI/Generated/`. |
| `root.type` | Root widget type (almost always `"CanvasPanel"`). |
| `root.name` | Name used as `parent` reference by child widgets. |

### Optional Top-Level Fields

| Field | Description |
|-------|-------------|
| `parentClass` | Native `UUserWidget` subclass path, e.g. `"/Script/WTBR.WTBRGeneratedHUDWidget"` (short form `"WTBR.WTBRGeneratedHUDWidget"` also accepted). Set as the WBP's parent class so `BindWidget`/`BindWidgetOptional` properties resolve immediately after import. Unresolvable classes warn and fall back to `UserWidget` (new asset) or keep the existing parent (in-place update). |

### Per-Widget Fields

| Field | Type | Description |
|-------|------|-------------|
| `type` | string | See supported types below. |
| `name` | string | Unique widget name. |
| `parent` | string | Parent widget name. Omit to attach to root. |
| `text` | string | Text for TextBlock or Button label. |
| `position` | `{x, y}` | Canvas position (CanvasPanel children only). |
| `size` | `{width, height}` | Canvas slot size (CanvasPanel children only). |
| `fontSize` | int | Font size for TextBlock / Button label. |
| `color` | string | Hex color: `"#RRGGBB"` or `"#RRGGBBAA"`. |
| `zOrder` | int | Z-order for CanvasPanel children. |
| `imagePath` | string | UE content path for Image brush, e.g. `"/Game/Textures/MyTex"`. |
| `isVariable` | bool | Expose the widget as a Blueprint variable. When omitted, widgets named `Txt_*`, `PB_*`, `Img_*`, or `Btn_*` are exposed automatically (binding-candidate convention). |

---

## Supported Widget Types (12)

| JSON `type` | UMG Widget | Notes |
|-------------|-----------|-------|
| `CanvasPanel` | `UCanvasPanel` | Root / absolute positioning |
| `TextBlock` | `UTextBlock` | `text`, `fontSize`, `color` |
| `Button` | `UButton` | Auto TextBlock child if `text` set |
| `Image` | `UImage` | `imagePath` for brush, `color` for tint |
| `Border` | `UBorder` | `color` sets brush color; single-child |
| `VerticalBox` | `UVerticalBox` | Auto-fill slots |
| `HorizontalBox` | `UHorizontalBox` | Auto-fill slots |
| `ProgressBar` | `UProgressBar` | `color` sets fill; default 50% |
| `ScrollBox` | `UScrollBox` | Vertical orientation default |
| `Overlay` | `UOverlay` | Stacked layers, fill alignment |
| `SizeBox` | `USizeBox` | `size` sets width/height override; single-child |
| `Spacer` | `USpacer` | `size` sets spacer dimensions |

---

## Known Limitations (MVP)

- **CanvasPanel only for positioning** — `position`, `size`, and `zOrder` are applied only when the
  parent is a `CanvasPanel`. Box-layout slots (VerticalBox, HorizontalBox, ScrollBox) use
  auto-fill defaults.
- **No font asset selection** — uses the default Slate font at the specified size.
- **No texture hot-reload** — `imagePath` must be a valid content path already cooked into the project.
- **No duplicate detection by content** - if a WBP with the same `widgetName` already exists, the
  importer prompts to update in place, duplicate with a suffix, or cancel. **Update in place**
  rebuilds only the widget tree inside the existing asset — the asset GUID, parent class (unless
  the JSON's `parentClass` overrides it), and references from other Blueprints all survive.
  Manual widget-tree edits made in UMG Designer are replaced by the JSON layout.
- **No event graph wiring** — D4-B guardrail: layout only, no Blueprint logic.
- **No style assets** — Button/Border visual styles use UE defaults; color sets `BackgroundColor` tint.
- **Widget tree structural edits after import** — use UMG Designer manually for hierarchy changes
  (confirmed by T3 spike: Python `find_objects` is not available in UE5.1).

---

## Next Steps

| Step | Description |
|------|-------------|
| **Slot padding support** | Add `padding` field for VerticalBox/HorizontalBox slots. |
| **Anchors support** | Add `anchors` field for CanvasPanelSlot min/max anchors. |
| **Font family override** | Add `fontFamily` field to select a named font from the project. |
| **Style presets** | Add a `stylePreset` field mapping to a named `UButtonStyle`/`UBorderBrush` asset. |
| ~~**Overwrite mode**~~ | ✅ Done (v1.1): update-in-place re-import preserves asset identity + parent class. |
| **Batch import** | Accept a folder path and import all `.json` files in one pass. |
| **WBP → JSON export** | Reverse direction: dump an existing WBP hierarchy back to JSON. |
| **Headless commandlet** | `-run=UMGJsonImport -file=X.json` for CI/automation regeneration. |

---

## v1.1 Upgrades (2026-07-06)

Three pipeline upgrades landed together:

1. **`parentClass` + `isVariable`** — the generated WBP can be born with its native
   parent class (`BindWidget`/`BindWidgetOptional` resolve immediately), and
   binding-candidate widgets (`Txt_`/`PB_`/`Img_`/`Btn_` prefixes, or an explicit
   `"isVariable": true`) are exposed as Blueprint variables automatically. All four
   `Examples/GameUI/*.json` files now declare their production parent class
   (`WTBRGeneratedHUDWidget` for the HUD, `WTBRBagLootWidget` for Bag/Loot).
2. **Update-in-place re-import** — choosing *Yes* on the existing-asset prompt now
   rebuilds the widget tree inside the same asset instead of force-deleting it.
   References, GUID, and parent class survive; open editors for the asset are closed
   automatically before the rebuild.
3. **HTML → JSON converter** — `WebPreview/converter.html` loads a prototype page in
   a same-origin iframe, measures the **rendered** DOM (`getBoundingClientRect` +
   `getComputedStyle`, ancestor transform scaling neutralized), and emits schema
   JSON: flat `RootCanvas` children with real pixel positions, hex-with-alpha colors,
   font sizes, and project-prefix names. `data-umg`/`data-umg-type`/`data-umg-name`
   attributes override the automatic classification. Verified against
   `Docs/UI_Prototypes/Vaelborne_InMatch_HUD.html`: 66 widgets, 0 validation errors,
   coordinates/colors matching the Phase 3.1 hand-measured values.

---

## Examples

| File | Widgets | Purpose |
|------|---------|---------|
| `Examples/WBP_MainMenu.json` | 7 | Basic menu: Border BG, title, 3 Buttons, ProgressBar, label |
| `Examples/WBP_AllSupportedWidgets_Test.json` | 29 | Stress test: all 12 types, nested containers, multiple parents |

---

## Phase 1 Validation Test Files

Invalid schema examples live in:

`Plugins/UMGJsonGenerator/Examples/Invalid/`

Use these files to verify that bad input does not crash Unreal Editor and that the importer logs clear messages through `LogUMGJsonImporter`.

| File | Expected behavior |
|------|-------------------|
| `WBP_Invalid_MissingWidgetName.json` | Import fails gracefully before asset creation with a missing `widgetName` error. |
| `WBP_Invalid_EmptyWidgetName.json` | Import fails gracefully before asset creation with an empty `widgetName` error. |
| `WBP_Invalid_WrongParent.json` | Widget with `parent: "NotExistParent"` is skipped, remaining valid widgets may still import. |
| `WBP_Invalid_UnsupportedType.json` | Widget with `type: "SuperCustomWidget"` is skipped with an unsupported type warning. |
| `WBP_Invalid_BadColor.json` | Invalid color values warn and fall back to default color. |
| `WBP_Invalid_DuplicateNames.json` | Duplicate widget name warns and the duplicate widget is skipped. |
| `WBP_Invalid_InvalidRootType.json` | Import fails gracefully before asset creation because root must be `CanvasPanel`. |
| `WBP_Invalid_MissingPositionOrSize.json` | CanvasPanel children missing `position` or `size` warn and use default slot values. |

Manual validation steps:

1. Open Unreal Editor with the plugin enabled.
2. Use the UMG JSON import action on each file in `Examples/Invalid/`.
3. Confirm the editor stays open and no crash occurs.
4. For failed validation before asset creation, confirm no generated WBP is created.
5. For partial validation cases, confirm remaining valid widgets import and the summary reports the asset path, widgets created, warnings count, and skipped count.
6. Open `Window -> Developer Tools -> Output Log`.
7. Filter by `LogUMGJsonImporter` and confirm the expected warning or error appears.

---

## Phase 2 Web Preview

Standalone Web Preview files live in:

`Plugins/UMGJsonGenerator/WebPreview/`

Open this file directly in a browser:

`Plugins/UMGJsonGenerator/WebPreview/index.html`

Use the file picker, drag/drop area, or raw JSON textarea to preview UMGJsonGenerator JSON before importing it into Unreal. The preview shows an approximate visual canvas, widget hierarchy, validation messages, and import-style stats.

Notes:

- This is a browser preview tool only.
- This is not an HTML/CSS-to-JSON converter.
- It does not create or edit `.uasset` files.
- Unreal import remains the source of truth for generated Widget Blueprints.

See `Plugins/UMGJsonGenerator/WebPreview/README.md` for manual test steps and limitations.

---

## Phase 3 Game UI — In-Match HUD

Real gameplay JSON files (not menu mockups) live in:

`Plugins/UMGJsonGenerator/Examples/GameUI/`

| File | Widgets | Purpose |
|------|---------|---------|
| `Examples/GameUI/WBP_HUD_Generated.json` | 70 | In-match HUD for WTBR / Vaelborne: Dominion — match summary, kill feed, minimap, zone/timer/phase, HP/Vael/quick item, Main/Sub Trigger cards, cancel prompt, crosshair, pickup prompt, hit marker/damage feedback. All widgets are flat children of `RootCanvas` (absolute position/size/zOrder) so every widget passes the CanvasPanel position/size validation with 0 warnings. |

Layout notes:

- 1920x1080, dark translucent glass panels, cyan main accent (`#20D9FF`), orange sub accent (`#FF4B2F`). See Phase 3.1 below for the exact palette pulled from the approved HTML/CSS reference.
- Input-bound labels use placeholder text (`[Input]`) instead of hardcoded physical keys — final key glyphs resolve from Enhanced Input at runtime.
- Center screen and bottom-right are kept clear by design; no gameplay-blocking full-screen background border is included (this is an overlay HUD, not a menu).

WebPreview check step:

1. Start a local static server for `Plugins/UMGJsonGenerator/WebPreview/` (e.g. `python -m http.server` from that folder) and open `index.html`.
2. Paste the contents of `WBP_HUD_Generated.json` into the RAW JSON box (local examples are not fetched automatically) and click **Load / Render**.
3. Confirm **Import Stats**: Widget = `WBP_HUD_Generated`, Resolution = `1920 x 1080`, Root = `RootCanvas`, Warnings = `0`, Skipped = `0`, Supported = `71`.

Unreal import step:

1. Open the project in Unreal Editor with the plugin enabled.
2. **Tools → Import UMG JSON...** and select `WBP_HUD_Generated.json`.
3. Confirm the generated WBP opens at `/Game/UI/Generated/WBP_HUD_Generated` with 0 warnings / 0 skipped in the import summary.

Phase 4 next step: **Binding + Runtime Test** — wire the `Txt_`/`PB_`/`Img_` widgets in this HUD to live gameplay state (HP, Vael, zone timer, kill feed, trigger cooldowns) and resolve `[Input]` placeholders from the Enhanced Input subsystem.

### Phase 3.1 — HUD Visual Fidelity Pass

`WBP_HUD_Generated.json` was re-laid-out to match the approved visual reference at
`Docs/UI_Prototypes/Vaelborne_InMatch_HUD.html` / `.css` (source of truth for panel
positions, sizes, colors, opacity, and hierarchy):

- Coordinates now follow the CSS exactly where the plugin schema allows: 26px edge gap,
  cluster widths of 290/348/306/398/326px, minimap 306x274, trigger cards 326x140, etc.
- Palette pulled from the CSS custom properties: panel glass `rgba(2,10,16,.58)` →
  `#020A1094`, trigger-card glass `rgba(4,16,24,.56)` → `#0410188F`, cyan `#20D9FF`,
  **orange sub-accent corrected to `#FF4B2F`** (was `#FF8C32` in Phase 3), yellow zone
  label `#FFC02E`, HP green `#7FF16D`, Vael blue `#15D7FF`, text `#E9FBFF`, muted
  `#8AB1BA`.
- Match summary and kill feed are now compact single-row/tight-stack panels instead of
  tall boxes, matching the mockup's density.
- HP and VAEL are now two separate card panels (`Border_PlayerVitals`, new
  `Border_VaelCard`) instead of one shared box, matching the mockup's `vital-card` /
  `vital-row` structure; `Border_QuickItem`'s background box was removed since the
  mockup's quick-item control is icon-only/transparent.
- Crosshair rebuilt as 4 short ticks (`Border_Crosshair_H`, new `Border_Crosshair_H2`,
  `Border_Crosshair_V`, new `Border_Crosshair_V2`) instead of 2 long bars, matching the
  mockup's `.crosshair span` geometry.
- Added 3 small left-accent bars (`Border_KillFeedAccent1-3`, decorative, non-binding) to
  echo the mockup's per-entry `border-left` accent on kill feed rows.
- All 37 `Txt_`/`PB_`/`Img_` binding candidate names are unchanged from Phase 3 — only
  position, size, color, and a couple of short text values (cancel/pickup prompt copy)
  were adjusted for fidelity.
- Not reproduced (out of JSON-schema scope): clip-path angular panel corners, box-shadow
  glow, backdrop blur, per-fragment kill-feed text coloring (killer/verb/victim), and
  decorative slot-diamond/icon glyphs — the plugin's Border/TextBlock widgets have no
  corner-cut, glow, or rich-text fields to express these.

### Phase 3.2 — Trigger Card Visual Fidelity Pass

`Border_MainTriggerCard` / `Border_SubTriggerCard` and their children were rebuilt to look
like tactical trigger cards instead of a plain box + 5 text lines, approximating the CSS
`.trigger-card` / `.trigger-content` / `.slot-diamonds` / `.binding` rules and the
`◇ ◆ ◇ ◇` active-slot-diamond convention with widget types the schema actually supports
(Border, Image, TextBlock, ProgressBar):

- **Icon slot**: `Border_MainTriggerIconBox`/`Border_SubTriggerIconBox` (72x72 tinted
  slot, cyan-dark for Main, orange-dark for Sub) + `Img_MainTriggerIcon`/
  `Img_SubTriggerIcon` (placeholder tint, no texture asset available) approximate the
  CSS `.trigger-icon` weapon-icon block.
- **Key chip**: `Border_MainTriggerKeyBox`/`Border_SubTriggerKeyBox` (44x32 faint chip,
  `rgba(255,255,255,.055)`→`#FFFFFF1F`) + new `Txt_MainTriggerKey`/`Txt_SubTriggerKey`
  (new Txt_ binding candidates, text `[Input]`) approximate the CSS `.binding` /
  `.trigger-binding` key glyph. **Not hardcoded** — placeholder only, per the input-binding
  rule already used elsewhere in this HUD.
- **Slot diamonds**: 4 small squares per card (`Border_{Main,Sub}TriggerDiamond1-4`,
  8x8) approximate the `◇ ◆ ◇ ◇` / `◇ ◇ ◆ ◇` active-slot indicator — the 2nd diamond is
  lit cyan for Main (slot 1 active) and the 3rd is lit orange for Sub (slot 2 active),
  rest are muted. Actual diamond glyph shape isn't renderable (no rotated-square/glyph
  support), so these are flat squares.
- **Card edge accents**: `Border_{Main,Sub}TriggerTopAccent`/`BottomAccent`/`LeftAccent`
  (2-3px strips, cyan `#20D9FFB8` for Main / orange `#FF4B2FB8` for Sub) approximate the
  CSS per-card `border-color` glow (`.main-trigger`/`.sub-trigger`) since the Border
  widget can only set one flat fill color, not a per-side border color.
- All 5 existing bindings per card (Label/Name/Cost/PB_Cooldown/State) were repositioned
  to make room for the icon column and key chip, but **kept their exact names** and
  values; name/cost font sizes were reduced slightly (22px/10px) to fit the new denser
  layout inside the unchanged 326x140 card footprint.
- Widget count: 54 → 76 (22 new decorative + binding widgets: 11 per card × 2 cards, of
  which `Txt_MainTriggerKey`/`Txt_SubTriggerKey` are new Txt_ binding candidates and the
  other 20 are decorative Border_/Img_ widgets).

Still not reproduced (schema limitation, unchanged from Phase 3.1): clip-path angular
card corners, box-shadow glow/blur, and true diamond/slash/shield icon glyph shapes —
approximated with flat rectangles and tint colors instead.

### Phase 3.3 — HUD Correction Pass (human review)

Follow-up corrections from a human visual review of the Phase 3.2 layout:

- **Trigger card stack lowered**: `Border_CancelPrompt`, both trigger cards, and every
  accent/icon/text child inside them were shifted down by 34px (e.g. Main card
  `y:700→734`, Sub card `y:858→892`) so the stack's bottom edge (`1032`) now lines up
  with the bottom-left HP/Vael stack's bottom edge, instead of sitting noticeably higher
  with a large empty gap below.
- **Zone number split into its own bindable widget**: added `Txt_ZoneValue` (sample
  text `"2"`, same yellow as `Txt_ZoneLabel`) next to `Txt_ZoneLabel = "ZONE"` — the
  number is no longer baked into the label string.
- **Phase changed from a progress bar to bindable text**: removed `PB_ZonePhase`
  (it only existed to represent phase, per the correction) and added `Txt_PhaseLabel =
  "PHASE"` + `Txt_PhaseValue = "3"` (separate label/value, cyan, right-aligned value) in
  the same zone-timer row, matching the CSS `.zone-strip` `PHASE 3` segment.
- **Trigger slot indicator changed from decorative squares to bindable text**: removed
  the 4 `Border_{Main,Sub}TriggerDiamond1-4` squares per card (8 widgets total) and
  replaced each group with a single `Txt_MainTriggerSlotIndicator` /
  `Txt_SubTriggerSlotIndicator` TextBlock showing the sample glyph string (`"◆ ◇ ◇ ◇"` /
  `"◇ ◆ ◇ ◇"`) — these are runtime-bindable to reflect the actual active slot (1-4) later,
  which 4 separate flat-color squares could not represent as cleanly.
- **"Ready" state text removed**: deleted `Txt_MainTriggerState` and
  `Txt_SubTriggerState` entirely (not left blank) since Phase 4 will not bind a ready
  state for now — the card's cooldown bar remains as the only runtime cooldown
  indicator.
- Widget count: 76 → 70 (net -6: -1 `PB_ZonePhase`, -8 diamond squares, +2 slot
  indicators, +1 `Txt_ZoneValue`, +2 phase texts, -2 state texts).
- Binding candidate list changes: **added** `Txt_ZoneValue`, `Txt_PhaseLabel`,
  `Txt_PhaseValue`, `Txt_MainTriggerSlotIndicator`, `Txt_SubTriggerSlotIndicator`;
  **removed** `Txt_MainTriggerState`, `Txt_SubTriggerState`, `PB_ZonePhase`. All other
  previously-listed binding widgets (`Txt_MainTriggerLabel/Name/Cost`,
  `PB_MainTriggerCooldown`, the Sub equivalents, `Txt_ZoneLabel`, `Txt_ZoneTimer`,
  `Txt_HPValue`, `Txt_VaelValue`, `Txt_PickupPrompt`, `Txt_QuickItem{Name,Count,Key}`,
  etc.) are unchanged.
- Assumption: sample placeholder values (`"2"` for zone, `"3"` for phase, `"◆ ◇ ◇ ◇"` /
  `"◇ ◆ ◇ ◇"` for slot indicators) are illustrative only, matching the review's explicit
  instruction not to treat them as fixed production data.

---

## Phase 3B / 3C — Bag and Corpse Loot UI JSON Mockups

Three new JSON mockups live alongside `WBP_HUD_Generated.json` in
`Plugins/UMGJsonGenerator/Examples/GameUI/`:

| File | Widgets (current) | Purpose |
|------|---------|---------|
| `WBP_BagPanel_Generated.json` | 71 | Right-side, full-height in-match Bag panel: title, capacity/weight, a 14-slot inventory grid (4 columns), and an optional bag-full warning line. |
| `WBP_CorpseLootPanel_Generated.json` | 47 | Contextual corpse/Trigger Cache loot panel positioned to the left of the Bag panel: title, subtitle, interact prompt, and an 8-row loot entry list (icon, name, type, action). |
| `WBP_BagLootLayer_Generated.json` | 119 | Composite preview showing both panels together at their final relative positions, plus a center-bottom `Txt_ContextHint` placeholder. Duplicates the Bag and Loot widgets directly in one JSON (the current schema has no nested-WBP-reference support), so it validates and previews independently of the other two files. |

Widget counts above reflect the Phase 3B.1/3C.1 visual fidelity pass (see that section
below) — original Phase 3B/3C counts were 62/44/107.

Source of truth used: `Docs/UI_Prototypes/Vaelborne_InMatch_Bag_Loot.html` / `.css`
(panel widths, edge gaps, colors, opacity) and `Docs/WTBR_D2_UMG_BAG_LOOT_CONVERSION_PLAN.md`
(widget hierarchy intent, request-only/read-only rules). This pass deliberately
implements a **simpler** widget set than the full D2 plan (no active-trigger cards,
reserve rows, or per-slot rarity/hover/selected states) — those are runtime/state
concerns for a later binding pass, not this visual-mockup pass.

Palette (from the CSS `:root` variables): panel glass `rgba(2,8,14,.74)` →
`#02080EBD`, cyan `#20D9FF`, orange `#FF4B2F`, yellow `#FFC02E`, text `#E9FBFF`,
muted `#8AB1BA`. Slot/row backgrounds use a flat dark tint (`#0F1E26` / `#0F1E2680`)
since the Border widget has no separate border-only/glow styling.

Input-bound text (`Txt_BagHint`, `Txt_CorpseLootPrompt`, `Txt_ContextHint`) uses
`[Input]` placeholders instead of hardcoding `RMB`/`F`/etc., per the same remappable-key
rule already used in `WBP_HUD_Generated.json`.

WebPreview check step:

1. Start a local static server rooted at `Plugins/UMGJsonGenerator/` (not just
   `WebPreview/`) so the page can `fetch()` the example JSON directly, e.g.
   `python -m http.server 8743 --directory Plugins/UMGJsonGenerator`, then open
   `http://localhost:8743/WebPreview/index.html`.
2. Paste or fetch the JSON into the RAW JSON box and click **Load / Render** for each
   of the three files.
3. Confirm **Import Stats** for each: Root = `RootCanvas`, Warnings = `0`, Skipped = `0`,
   Unsupported = `0` — Widgets = `71` / `47` / `119` respectively (current counts;
   see Phase 3B.1/3C.1 below).

Unreal import step:

1. Open the project in Unreal Editor with the plugin enabled.
2. **Tools → Import UMG JSON...** and select each of the three files in turn.
3. Confirm each generated WBP opens at `/Game/UI/Generated/<widgetName>` with 0
   warnings / 0 skipped in the import summary.

Binding is intentionally deferred: these three JSONs are visual mockups only, matching
the same "accepted as usable mockup, binding comes later" status as
`WBP_HUD_Generated.json`. A Bag/Loot binding audit plan and a read-only ViewModel
component (`UWTBRBagLootViewModelComponent`) were added in a later pass — see
`Plugins/UMGJsonGenerator/Examples/GameUI/WBP_BagLoot_BindingPlan.md` — but the native
`UUserWidget` parent class and WBP-side parent-class assignment remain paused (see
Phase 3B.1/3C.1 below: a visual fidelity correction was prioritized ahead of
continuing that binding work).

---

## Phase 3B.1 / 3C.1 — Bag + Corpse Loot Visual Fidelity Pass

Status: human review found the Phase 3B/3C mockups did not visually match the
approved prototype closely enough. This pass corrected `WBP_BagPanel_Generated.json`,
`WBP_CorpseLootPanel_Generated.json`, and `WBP_BagLootLayer_Generated.json` against
the actual **rendered** HTML/CSS prototype (not just its CSS source text) before any
further Bag/Loot binding or native-widget work continues.

Source of truth: `Docs/UI_Prototypes/Vaelborne_InMatch_Bag_Loot.html` / `.css`,
opened in a real browser and measured with `getBoundingClientRect()` /
`getComputedStyle()` at 1920x1080 — every position/size value below is a
browser-measured real pixel, not an estimate from reading the CSS text.

Key correction: the first fidelity attempt shrank the Bag panel to a "content-fit"
height (~560px) to avoid an empty-looking box, which inadvertently moved the
inventory grid to the wrong vertical position relative to the reference. The real
`.bag-panel` spans nearly the full screen height (measured `y=16, h=1048`) because it
also contains Active Trigger Card and Reserve Row sections this pass does not
implement (no bound widgets exist for those, and inventing placeholder trigger/reserve
data would violate the "do not invent gameplay state" rule). This pass restores the
full real panel height and positions the grid/footer at their **true measured Y
coordinates** (grid at `y=468`, footer at `y=1018`+), leaving the unimplemented
middle section as clean glass rather than compressing the whole panel upward.

Real measured values used (selected):

- Bag panel: `x=1538, y=16, w=356, h=1048`. Header `h=49`. Cap row `y=66, h=25`.
  Section separator `y=441`. Inventory grid `x=1555, y=468`, 4 columns, cell
  `75x75`, step `83` (75 + 8 gap) — a true 4x4 (16-cell) grid, confirmed via
  `document.querySelectorAll('.bag-slot').length === 16`. Footer volume label
  `y=1027`, bar track `y=1046, h=5`.
- Corpse Loot panel: `x=1250, y=120, w=276`. Real page only shows 6 sample rows at
  row height `52` / row step `55`; height extended by exactly 2 row-steps (110px)
  to fit the 8 required bound rows at the *same* real row metrics, not
  re-proportioned. Row content offsets: icon `+9,+8` (36x36), name `+57,+10`
  (169x17, 13px), type `+57,+29` (169x14, 10px), action `+234,+17` (28x19).
- Colors confirmed via `getComputedStyle(...).color`/`.backgroundColor` on live
  elements: cyan `rgb(32,217,255)` = `#20D9FF`, text `rgb(233,251,255)` = `#E9FBFF`,
  muted `rgb(138,177,186)` = `#8AB1BA`, panel glass `rgba(2,8,14,0.74)` = `#02080EBD`,
  header tint `rgba(32,217,255,0.055)` ≈ `#20D9FF0F`.

Files modified (widget counts): `WBP_BagPanel_Generated.json` 68→71,
`WBP_CorpseLootPanel_Generated.json` 47→47 (repositioned only, count unchanged),
`WBP_BagLootLayer_Generated.json` 116→119.

All required binding widget names (`Txt_BagTitle/Capacity/Weight/Hint/Warning`, all
14×`Txt_BagSlotNNName/Count`+`Img_BagSlotNNIcon`, `Txt_CorpseLootTitle/Subtitle/Prompt`,
all 8×`Txt_LootEntryNNName/Type/Action`+`Img_LootEntryNNIcon`) were preserved exactly;
only new decorative `Border_`/static-label widgets were added (header/footer tints,
dividers, a static "VOLUME"/"INVENTORY ITEMS" caption, 2 reserved-cell borders
completing the real 16-cell grid) or repositioned.

Unsupported CSS features (clip-path angular corners, box-shadow glow, backdrop blur,
gradients, animations) are still approximated with flat supported widgets only
(Border/TextBlock/Image), per the schema's existing limitations documented in the
HUD phases.

WebPreview validation (confirmed live, `fetch()`-loaded, not just scripted): all
three files — Warnings `0`, Skipped `0`, Unsupported `0`.

Binding remains intentionally deferred pending this visual fidelity approval —
Phase 4C-BagLoot native widget work is paused per explicit instruction.

---

## Compile Fix Notes (UE 5.1.1)

| Issue | Fix |
|-------|-----|
| `FFileHelper` not found | Added `#include "Misc/FileHelper.h"` |
| `UPackage::SavePackage` returns `bool` | Use `UPackage::Save` (returns `FSavePackageResultStruct`) |
| `UTextBlock::Font` deprecated | Use `GetFont()` / `SetFont()` |
| `USpacer::Size` deprecated | Use `SetSize()` |
| `Cast<UOverlaySlot>` incomplete type | Added `#include "Components/OverlaySlot.h"` |
