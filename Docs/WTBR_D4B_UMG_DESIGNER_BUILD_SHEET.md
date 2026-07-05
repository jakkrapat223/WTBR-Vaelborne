# WTBR D4-B-Spec: UMG Designer Build Sheet — Bag + Corpse Loot

Project: WTBR / Vaelborne: Dominion  
Engine: Unreal Engine 5.1.1  
Pass: D4-B-Spec — Docs only  
Baseline: `fc98c41` — T2: Add UMG template clone spike  
Date: 2026-07-05

---

## 1. Purpose / Scope

This document is a **docs-only build sheet** for the manual Unreal Engine UMG Designer work required
to complete the visual layout specification of the five approved production Widget Blueprint assets
for the Bag + Corpse Loot in-match UI overlay.

D4-B-Spec translates:

- The D1 HTML/CSS visual reference prototype into concrete UMG Designer steps
- The D2 UMG conversion plan widget hierarchy into named widget trees
- The D3 approval gate constraints into Designer-level guardrails
- The D4-A skeleton assets into fully styled read-only display widgets

A human developer must follow this document manually in Unreal Editor (UMG Designer tab).
This is not automation — it is step-by-step manual construction guidance.

**Scope includes:**

- Visual layout and hierarchy for all 5 approved WBP assets
- Read-only ViewModel / snapshot binding notes for future implementation
- Placeholder text rules
- Visibility state definitions
- Manual UE save checklist
- Post-pass validation checklist

**Scope excludes everything listed in Section 17.**

---

## 2. Hard Guardrails

These rules apply throughout D4-B. Violation of any guardrail voids the pass.

| Rule | Detail |
|------|--------|
| No Event Graph | Do not open or edit the Event Graph in any WBP during D4-B. |
| No gameplay mutation | Widgets display data only. No Blueprint node may write to gameplay state. |
| No binding logic edits | Do not create or edit Blueprint binding functions, Event Graph logic, or runtime binding code in this pass. |
| No drag/drop logic | Do not wire any drag/drop events or `OnDrop` handlers. |
| No server request integration | No RPC calls, no `Server_` functions in any Blueprint graph. |
| No generated WBP changes | Do not touch `Content/UI/Sandbox/WBP_WTBRToolingSpike_Test` or `WBP_WTBRBagSlot_CloneSpike`. |
| No key label hardcoding | Do not type `F`, `X`, `Q`, `E`, `LMB`, `RMB`, or `4` into any TextBlock text property. Any visible input label must come from Input Mapping / Enhanced Input resolution. |
| No Save All | Do not use **File → Save All**. Save only the 5 approved WBP files individually. |
| No map save | Do not save ThirdPersonMap, any `.umap`, or any External Actor file. |
| No binary asset creation | Do not import textures, icons, sounds, or any external asset. |
| Read-only binding references only | Any future binding must read from ViewModel/snapshot only. No binding writes back to gameplay state. Do not implement binding logic in D4-B-Spec. |
| No `.uasset` staging | Do not `git add` any `.uasset` file without separate human approval. |

---

## 3. Source References

| Reference | File | Role |
|-----------|------|------|
| D1 HTML visual reference | `Docs/UI_Prototypes/Vaelborne_InMatch_Bag_Loot.html` | Visual prototype — layout, states, component structure |
| D1 CSS style sheet | `Docs/UI_Prototypes/Vaelborne_InMatch_Bag_Loot.css` | Color palette, sizing, typography |
| D2 UMG conversion plan | `Docs/WTBR_D2_UMG_BAG_LOOT_CONVERSION_PLAN.md` | Widget hierarchy, binding fields, request-only rules |
| D3 approval gate | `Docs/WTBR_D3_UMG_BAG_LOOT_ASSET_APPROVAL_GATE.md` | Approved WBP targets, path convention, guardrails |
| D4-A commit | `73bc151` — D4-A: Add bag loot WBP skeleton assets | Source WBP skeleton files to be built out in this pass |
| T1.1 spike | `Docs/WTBR_T1_UMG_LAYOUT_TOOLING_SPIKE.md` | Confirmed: widget tree structural editing via Python is BLOCKED in UE5.1.1; manual Designer required |
| T2 spike | `Docs/WTBR_T2_UMG_TEMPLATE_CLONE_SPIKE.md` | Confirmed: template clone via Python works; `WBP_WTBRBagSlot_CloneSpike` is a verified reference clone |
| JSON layout spec | `Docs/WTBR_T1_UMG_LAYOUT_SPEC_EXAMPLE.json` | UMG slot/property format reference for Designer |

### 3.1 Approved WBP Targets (D4-B scope)

The following 5 production WBP assets were created in D4-A. D4-B builds out their Designer layout. Read-only binding fields are documented as future references only. No other WBP file is in scope.

| Asset | File path | UE content path |
|-------|-----------|-----------------|
| `WBP_WTBRBagLootLayer` | `Content/UI/HUD/WBP_WTBRBagLootLayer.uasset` | `/Game/UI/HUD/WBP_WTBRBagLootLayer` |
| `WBP_WTBRBagPanel` | `Content/UI/HUD/WBP_WTBRBagPanel.uasset` | `/Game/UI/HUD/WBP_WTBRBagPanel` |
| `WBP_WTBRBagSlot` | `Content/UI/HUD/WBP_WTBRBagSlot.uasset` | `/Game/UI/HUD/WBP_WTBRBagSlot` |
| `WBP_WTBRCorpseLootPanel` | `Content/UI/HUD/WBP_WTBRCorpseLootPanel.uasset` | `/Game/UI/HUD/WBP_WTBRCorpseLootPanel` |
| `WBP_WTBRCorpseLootEntry` | `Content/UI/HUD/WBP_WTBRCorpseLootEntry.uasset` | `/Game/UI/HUD/WBP_WTBRCorpseLootEntry` |

---

## 4. Global Visual Style

### 4.1 Aesthetic Identity

**Futuristic Tactical Battle Royale** — dark, translucent glass panels; cyan/teal main accent; orange
sub-accent; angular geometry with hard edges.

**Avoid:** medieval RPG styling, brown/gold inventory aesthetics, fantasy ornamentation,
full-opacity opaque panels, centered full-screen inventory popups, rounded "card" softshadows.

### 4.2 Color Palette

All hex values are **sRGB**. In the UMG Color Picker, switch to sRGB mode before entering hex values.
Alpha is expressed as 0–255 (A=255 = fully opaque).

| Token | Hex (sRGB) | Alpha | Role |
|-------|-----------|-------|------|
| `--panel-bg` | `#02080E` | A=189 (74%) | Panel glass dark background |
| `--cyan` | `#20D9FF` | A=255 | Main accent — selected, active, header titles |
| `--cyan-soft` | `#20D9FF` | A=107 (42%) | Panel border, badge border |
| `--cyan-dim` | `#20D9FF` | A=36 (14%) | Badge fill, subtle highlight fill |
| `--orange` | `#FF4B2F` | A=255 | Sub-trigger accent — sub-slot labels, close hover |
| `--orange-soft` | `#FF4B2F` | A=117 (46%) | Sub-slot border |
| `--orange-dim` | `#FF4B2F` | A=36 (14%) | Sub-slot badge fill |
| `--text` | `#E9FBFF` | A=255 | Primary text |
| `--muted` | `#8AB1BA` | A=255 | Secondary / label text, empty slot text |
| `--yellow` | `#FFC02E` | A=255 | Bag full warning, locked badge, Legendary rarity |
| `--red` | `#FF2438` | A=255 | Warn glow (bag-full border emphasis) |
| Panel dim overlay | `#000000` | A=71 (28%) | Context background dim |

### 4.3 Rarity Color Palette

| Rarity | Hex (sRGB) | Use |
|--------|-----------|-----|
| Common | `#8AB1BA` | Icon tint, meta label |
| Uncommon | `#52D46E` | Icon tint, meta label |
| Rare | `#4FA4FF` | Icon tint, meta label |
| Epic | `#AE6FFF` | Icon tint, meta label |
| Legendary | `#FFC02E` | Icon tint, meta label |

### 4.4 Typography

| Property | Value |
|----------|-------|
| Primary font | Segoe UI (Windows default); fallback Rajdhani if available in UE5 project |
| Header labels | Size 10–12, Weight ExtraBold / Black, Letter Spacing +3 to +6 |
| Panel title text | Size 13–16, Weight Bold to Black |
| Body / meta text | Size 10–11, Weight Regular / SemiBold |
| Count / quantity | Size 13–14, Weight Black |
| Secondary labels | Size 10–11, Color `#8AB1BA` |

In UMG TextBlock, "Letter Spacing" is the `Kerning` property. Increase by +1 to +4 to simulate the
D1 CSS `letter-spacing: 0.08–0.20em` effect.

### 4.5 Panel Frame Style

| D1 CSS property | D1 value | UMG approach for D4-B |
|----------------|----------|-----------------------|
| Background | `rgba(2,8,14,0.74)` | `Border` widget, Brush Tint = `#02080E` A=189 |
| Panel border | 1 px solid `#20D9FF` A=107 | Outer `Border` with 1 px inner padding using a background-colored inner Border, or accept visual approximation |
| Corner cut | 12 px octagonal `clip-path` | Not supported natively in UMG. Use solid rect for D4-B. Defer art treatment to D4-F polish pass. |
| Backdrop blur | `blur(8px)` | Not natively available in UMG 5.1.1 without a custom material. Skip for D4-B. |
| Idle glow | `box-shadow: 0 0 14px rgba(32,217,255,0.10)` | Skip for D4-B. Add via material/image in D4-F. |

### 4.6 Panel Sizing Reference (from D1 CSS)

| Element | D1 value | UMG value |
|---------|----------|-----------|
| Bag Panel width | 356 px | SizeBox Width Override = 356 |
| Loot Panel width | 276 px | SizeBox Width Override = 276 |
| Loot Panel max-height | 660 px | SizeBox Max Desired Height = 660 |
| Panel H padding | 16 px | Padding L/R = 16 |
| Panel V padding | 12 px | Padding T/B = 12 |
| Bag grid columns | 4 | UniformGridPanel columns = 4 |
| Bag grid slot gap | 8 px | SlotPadding all sides = 4 (half-gap on each side) |
| Right margin (from screen edge) | 26 px | Overlay slot Padding Right = 26 |
| Top/Bottom margin | 16 px | Overlay slot Padding Top/Bottom = 16 |
| Gap between panels | 12 px | HBox slot Padding Right = 12 on Loot Panel slot |

---

## 5. WBP_WTBRBagLootLayer — Manual Designer Layout Steps

**File:** `Content/UI/HUD/WBP_WTBRBagLootLayer.uasset`  
**Role:** Container layer. Groups `WBP_WTBRCorpseLootPanel` and `WBP_WTBRBagPanel` for the in-match
contextual overlay. Handles show/hide and viewport anchor only. Contains no display content itself.  
**D1 reference:** `.panels-group`, `.context-dim`, `.bag-canvas`

### Step L1 — Open Widget

In Content Browser → `Content/UI/HUD/` → double-click `WBP_WTBRBagLootLayer` to open in UMG Designer.

### Step L2 — Set Root Canvas Panel

1. In the Hierarchy panel, confirm or set the root widget type to `Canvas Panel`.
2. Rename root to `Canvas_LayerRoot`.

### Step L3 — Add Full-Screen Overlay

1. Under `Canvas_LayerRoot`, add an `Overlay` widget.
2. Name it `Overlay_LayerRoot`.
3. In its Canvas Panel Slot (Details panel):
   - Anchors: Select the **full-stretch** preset (min anchor = 0,0 / max anchor = 1,1).
   - Left, Top, Right, Bottom offsets: all = 0.
   - Z Order: 0.
4. This Overlay fills the full viewport.

### Step L4 — Add Context Dim Layer

1. As the **first child** under `Overlay_LayerRoot`, add a `Border`.
2. Name it `Border_ContextDim`.
3. Overlay Slot: Horizontal Alignment = Fill, Vertical Alignment = Fill.
4. Brush Tint: `#000000`, Alpha = 71 (28%).
5. Set `Is Enabled` = false (dim is background-only, receives no input).
6. This replicates D1's `.context-dim` overlay.

### Step L5 — Add Panels Group

1. As the **second child** under `Overlay_LayerRoot`, add a `Horizontal Box`.
2. Name it `HBox_PanelsGroup`.
3. Overlay Slot:
   - Horizontal Alignment = Right
   - Vertical Alignment = Fill
   - Padding: Top=16, Bottom=16, Left=0, Right=26

### Step L6 — Add Child Panel Widgets

1. Under `HBox_PanelsGroup`, add two child slots in order (left to right):
   - **Slot A — Corpse Loot Panel:**
     - Add `WBP_WTBRCorpseLootPanel`.
     - HBox Slot: Size Fill = 0 (auto-size), Vertical Alignment = Top.
     - HBox Slot Padding: Right = 12.
   - **Slot B — Bag Panel:**
     - Add `WBP_WTBRBagPanel`.
     - HBox Slot: Size Fill = 0 (auto-size), Vertical Alignment = Fill.
2. This matches D1's right-aligned `.panels-group` with Loot left of Bag.

### Step L7 — Default Visibility

1. Select `Overlay_LayerRoot`.
2. Set Visibility = `Hidden`.
3. Future read-only binding note: this will later be driven by `bIsBagLayerActive` (see Section 12.2). Do not implement binding logic in D4-B-Spec.

### Step L8 — Save

Use **Ctrl+S** on the UMG Designer tab to save `WBP_WTBRBagLootLayer` only.
Do not use File → Save All.

---

## 6. WBP_WTBRBagPanel — Manual Designer Layout Steps

**File:** `Content/UI/HUD/WBP_WTBRBagPanel.uasset`  
**Role:** Right-side full-height in-match bag view. Contains trigger cards, reserve rows, bag grid,
header, capacity row, and footer.  
**D1 reference:** `.bag-panel`, `.bag-header`, `.bag-glyph`, `.bag-title`, `.bag-cap-row`,
`.active-card.active-main`, `.active-card.active-sub`, `.reserve-section`, `.bag-grid`, `.bag-footer`

### Step P1 — Open Widget

Open `WBP_WTBRBagPanel` in UMG Designer.

### Step P2 — Root SizeBox

1. Set root to `Size Box`, name it `SizeBox_BagRoot`.
2. Width Override: checked, value = **356**.
3. Height Override: unchecked (auto height from children).

### Step P3 — Glass Frame Border

1. Under `SizeBox_BagRoot`, add `Border`, name it `Border_BagGlass`.
2. Brush Tint: `#02080E` Alpha = 189 (74%).
3. Padding: Left=16, Right=16, Top=12, Bottom=12.
4. Horizontal Fill / Vertical Fill.

### Step P4 — Main Vertical Box

1. Under `Border_BagGlass`, add `Vertical Box`, name it `VBox_BagContent`.
2. This is the flex-column root for all bag content.

### Step P5 — Bag Header Row

1. Under `VBox_BagContent`, add `Horizontal Box`, name it `HBox_BagHeader`.
   - VBox Slot Padding: Bottom=6.

2. Under `HBox_BagHeader`, add the following children:

   **a. Bag Glyph badge**
   - Add `Border`, name it `Border_BagGlyph`.
   - Wrap in `Size Box` (parent SizeBox_BagGlyph): Width=28, Height=28.
   - Border Brush Tint: transparent (`#000000` A=0).
   - Border color: `#20D9FF` A=89 (35%).
   - Padding: Left=0, Right=0, Top=0, Bottom=0.
   - Inside `Border_BagGlyph`, add `TextBlock`, name `Text_BagGlyph`:
     - Text: `BG` (placeholder — replaced by icon art in D4-F)
     - Font Size: 10, Weight: Black, Color: `#20D9FF`
     - Letter Spacing (Kerning): +4
     - Alignment: Center / Center

   **b. Bag Title**
   - Add `TextBlock`, name it `Text_BagTitle`.
   - HBox Slot: Size Fill = 1.0.
   - HBox Slot Padding: Left=8.
   - Text: `BAG`
   - Font Size: 15, Weight: Black, Color: `#20D9FF`
   - Letter Spacing: +6

   **c. Close Button**
   - Add `Button`, name it `Btn_BagClose`.
   - Wrap in `Size Box`: Width=28, Height=28.
   - Style — Normal: `#FFFFFF` A=10 (4%); Hovered: `#FF4B2F` A=36 (14%); Pressed: `#FF4B2F` A=77 (30%).
   - Inside `Btn_BagClose`, add `TextBlock`, name `Text_CloseGlyph`:
     - Text: `✕` (visual glyph only — not a key label)
     - Font Size: 12, Weight: Bold, Color: `#8AB1BA`
     - Alignment: Center / Center
   - **D4-C note:** Btn_BagClose click will dispatch `RequestCancelCurrentUIOrAction()`. Not wired in D4-B.

### Step P6 — Header Separator

1. Under `VBox_BagContent`, add `Border`, name it `Border_HeaderSep`.
   - Brush Tint: `#20D9FF` A=56 (22%).
   - SizeBox or Slot: Height=1.
   - Horizontal Fill.
   - VBox Slot Padding: Bottom=4.

### Step P7 — Capacity Row

1. Under `VBox_BagContent`, add `Border`, name it `Border_CapRowBG`.
   - Brush Tint: `#000000` A=46 (18%).
   - Padding: Left=0, Right=0, Top=5, Bottom=5.
   - VBox Slot Padding: Bottom=4.

2. Inside `Border_CapRowBG`, add `Horizontal Box`, name it `HBox_CapRow`.
   - Align children center.

3. Under `HBox_CapRow`, add the following `TextBlock` children in order:
   - `Text_SlotCur`: Text=`12`, Size=11, Weight=ExtraBold, Color=`#E9FBFF`
   - `Text_SlotSep`: Text=`/`, Size=11, Color=`#E9FBFF` A=102 (40%)
   - `Text_SlotMax`: Text=`14`, Size=11, Color=`#8AB1BA`
   - `Text_SlotLabel`: Text=` SLOTS`, Size=11, Weight=Bold, Color=`#8AB1BA`, Kerning=+2
   - `Text_CapDivider`: Text=`  |  `, Size=11, Color=`#E9FBFF` A=77 (30%)
   - `Text_VolCur`: Text=`48`, Size=11, Weight=ExtraBold, Color=`#E9FBFF`
   - `Text_VolSep`: Text=`/`, Size=11, Color=`#E9FBFF` A=102 (40%)
   - `Text_VolMax`: Text=`60`, Size=11, Color=`#8AB1BA`
   - `Text_VolLabel`: Text=` VOL`, Size=11, Weight=Bold, Color=`#8AB1BA`, Kerning=+2

### Step P8 — Main Active Trigger Card

1. Under `VBox_BagContent`, add `Vertical Box`, name it `VBox_ActiveMain`.
   - VBox Slot Padding: Top=8.

2. Under `VBox_ActiveMain`:

   **a. Label**
   - Add `TextBlock`, name `Text_ActiveMainLabel`.
   - Text: `MAIN ACTIVE`, Size=10, Weight=ExtraBold, Color=`#20D9FF`, Kerning=+4.
   - VBox Slot Padding: Bottom=5.

   **b. Card row (framed box)**
   - Add `Border`, name it `Border_ActiveMainRow`.
   - Brush Tint: `#20D9FF` A=10 (4%).
   - Border color (via padding trick or border property): `#20D9FF` A=143 (56%).
   - Padding: all=8.

   **c. Inside `Border_ActiveMainRow`, add `Horizontal Box` `HBox_MainCardRow`.**

   Under `HBox_MainCardRow`:

   - **Icon cell:** `Size Box` (Width=46, Height=46) → `Border` `Border_MainTriggerIcon`:
     - Brush Tint: `#FFFFFF` A=10 (4%).
     - Border: `#FFFFFF` A=31 (12%).
     - Inside: `TextBlock` `Text_MainTriggerIconToken` — Text=`MT`, Size=14, Weight=Black, Color=`#20D9FF`, Alignment=Center.
     - HBox Slot Padding: Right=10.

   - **Details column:** `Vertical Box` `VBox_MainTriggerDetails`:
     - HBox Slot Size Fill=1.0.
     - `TextBlock` `Text_MainTriggerName`: Text=`[Trigger Name]`, Size=16, Weight=Bold, Color=`#20D9FF`.
     - `TextBlock` `Text_MainTriggerSub`: Text=`Blade | Slot M1`, Size=10, Color=`#8AB1BA`, Kerning=+2.

   - **Slot badge:** `Size Box` (Width=30, Height=28) → `Border` `Border_MainSlotBadge`:
     - Brush Tint: `#20D9FF` A=36 (14%).
     - Border: `#20D9FF` A=107 (42%).
     - Inside: `TextBlock` `Text_MainSlotBadge` — Text=`M1`, Size=11, Weight=Black, Color=`#20D9FF`, Alignment=Center.

### Step P9 — Sub Active Trigger Card

1. Under `VBox_BagContent`, add `Vertical Box`, name it `VBox_ActiveSub`.
   - VBox Slot Padding: Top=8.
2. Mirror Step P8 exactly, substituting **orange** for cyan everywhere:
   - `Text_ActiveSubLabel`: Text=`SUB ACTIVE`, Color=`#FF4B2F`
   - `Border_ActiveSubRow`: Brush Tint `#FF4B2F` A=10; border `#FF4B2F` A=143
   - `Text_SubTriggerIconToken`: Color=`#FF4B2F`
   - `Text_SubTriggerName`: Text=`[Trigger Name]`, Color=`#FF4B2F`
   - `Text_SubTriggerSub`: Text=`Blade | Slot S1`, Color=`#8AB1BA`
   - `Border_SubSlotBadge`: Brush Tint `#FF4B2F` A=36; border `#FF4B2F` A=107
   - `Text_SubSlotBadge`: Text=`S1`, Color=`#FF4B2F`

### Step P10 — Main Reserve Row

1. Under `VBox_BagContent`, add `Vertical Box`, name it `VBox_ReserveMain`.
   - VBox Slot Padding: Top=8.
2. Under `VBox_ReserveMain`:
   - `TextBlock` `Text_ReserveMainLabel`: Text=`MAIN RESERVE`, Size=10, Weight=ExtraBold, Color=`#8AB1BA`, Kerning=+3.
   - VBox Slot Padding: Bottom=5.
   - `Horizontal Box` `HBox_ReserveMainRow`.
3. Under `HBox_ReserveMainRow`, add **3 reserve slot cells** (M2, M3, M4):
   - Each cell: `Size Box` → `Border` `Border_MainSlotN` (N=2/3/4).
   - HBox Slot: Size Fill=1.0. Padding Right=6 (skip Right on last cell).
   - **Filled slot style:** Brush Tint `#FFFFFF` A=6 (2.5%); border `#20D9FF` A=82 (32%).
   - **Empty/reserved slot style:** Brush Tint `#000000` A=51 (20%); border `#8AB1BA` A=56 (22%), dashed feel (opacity 62%).
   - Inside each Border, add `Vertical Box`:
     - `TextBlock` `Text_MainSlotNIndex`: Text=`M2`/`M3`/`M4`, Size=10, Color=`#20D9FF` A=184 (72%), Kerning=+1, Alignment=Center.
     - `TextBlock` `Text_MainSlotNName`: Text=`[Name]` or `RES` (empty), Size=11, Weight=SemiBold, Color=`#E9FBFF`, Alignment=Center.

### Step P11 — Sub Reserve Row

1. Under `VBox_BagContent`, add `Vertical Box`, name it `VBox_ReserveSub`.
   - VBox Slot Padding: Top=8.
2. Mirror Step P10 with **orange** for cyan everywhere:
   - `Text_ReserveSubLabel`: Text=`SUB RESERVE`
   - Border filled style: border `#FF4B2F` A=82 (32%)
   - `Text_SubSlotNIndex`: Color=`#FF4B2F` A=184 (72%)
   - Slots: S2, S3, S4.

### Step P12 — Section Separator + Inventory Label

1. Under `VBox_BagContent`, add `Border` `Border_InvSep`:
   - Brush Tint: `#20D9FF` A=31 (12%). Height=1. Horizontal Fill.
   - VBox Slot Padding: Top=10.
2. Under `VBox_BagContent`, add `TextBlock` `Text_InvLabel`:
   - Text: `INVENTORY ITEMS`, Size=10, Weight=ExtraBold, Color=`#8AB1BA`, Kerning=+4.
   - VBox Slot Padding: Top=8, Bottom=4.

### Step P13 — Bag Grid (UniformGridPanel — 16 cells)

1. Under `VBox_BagContent`, add `Uniform Grid Panel`, name it `Grid_BagInventory`.
   - Slot Padding (applied to all child slots): 4 on each side (= half of D1's 8 px gap).
2. Add **16 child widgets** in row-major order (Row = index / 4, Column = index % 4):
   - **Cells 0–13 (14 active inventory slots):** Add `WBP_WTBRBagSlot` in each.
   - **Cells 14–15 (2 reserved visual cells):** For each, add a `Border` `Border_ReservedCellN`:
     - Brush Tint: `#000000` A=77 (30%); border `#8AB1BA` A=56 (22%).
     - Inside: `TextBlock` `Text_ReservedN` — Text=`RES`, Size=11, Weight=Black, Color=`#8AB1BA` A=158 (62%), Alignment=Center.

### Step P14 — Volume Fill Bar Footer

1. Under `VBox_BagContent`, add `Spacer` `Spacer_FooterPush` with Size Fill = 1.0 to push footer to bottom.
2. Add `Border` `Border_FooterSep`:
   - Brush Tint: `#20D9FF` A=31 (12%). Height=1. Horizontal Fill.
   - VBox Slot Padding: Top=8.
3. Add `Vertical Box` `VBox_BagFooter`.
   - VBox Slot Padding: Top=8, Bottom=4.
4. Under `VBox_BagFooter`:

   **a. Volume header row**
   - `Horizontal Box` `HBox_VolHeader`.
   - `TextBlock` `Text_VolBarLabel`: Text=`VOLUME`, Size=10, Weight=ExtraBold, Color=`#8AB1BA`, Kerning=+3. HBox Slot Size Fill=1.0.
   - `TextBlock` `Text_VolBarValue`: Text=`48 / 60`, Size=10, Weight=ExtraBold, Color=`#E9FBFF`. HBox Slot HAlign=Right.

   **b. Progress bar**
   - `Progress Bar` `ProgressBar_Volume`.
   - Fill Color Start/End: `#20D9FF` A=140 (55%) → `#20D9FF` A=255 (gradient look if available, or flat `#20D9FF`).
   - Background Color: `#FFFFFF` A=26 (10%).
   - Bar Fill Type: Left To Right.
   - Height override: 5.
   - Future read-only binding note: Percent should come from `CurrentVolume / MaxVolume` (read-only float, 0–1). Do not implement binding logic in D4-B-Spec.

   **c. Bag Full Warning row (Hidden by default)**
   - `Horizontal Box` `HBox_BagFullWarn`.
   - Visibility: **Hidden** (alternate state only — see Section 14).
   - HBox Slot Padding: Top=6.
   - `TextBlock` `Text_BagWarnIcon`: Text=`!`, Size=11, Color=`#FFC02E`.
   - `TextBlock` `Text_BagWarnText`: Text=`BAG FULL / CANNOT TRANSFER`, Size=11, Weight=ExtraBold, Color=`#FFC02E`, Kerning=+3.

### Step P15 — Save

Save `WBP_WTBRBagPanel` only (Ctrl+S on Designer tab).

---

## 7. WBP_WTBRBagSlot — Manual Designer Layout Steps

**File:** `Content/UI/HUD/WBP_WTBRBagSlot.uasset`  
**Role:** Individual inventory slot cell. Leaf widget. Displays item icon token, stack count,
optional locked badge, optional tooltip. Visual state driven by read-only snapshot flags.  
**D1 reference:** `.bag-slot`, `.bag-slot.filled`, `.bag-slot.empty`, `.bag-slot.reserved`,
`.bag-slot--hover`, `.bag-slot--selected`, `.bag-slot--locked`, `.bag-slot--drag-target`,
`.bag-slot--cannot-transfer`

### Step S1 — Open Widget

Open `WBP_WTBRBagSlot` in UMG Designer.

### Step S2 — Root Size Box

1. Set root to `Size Box`, name it `SizeBox_SlotRoot`.
   - Min Desired Width: 60. Min Desired Height: 60. (The `UniformGridPanel` in the parent determines actual cell size; min ensures a baseline square.)

### Step S3 — Overlay Root

1. Under `SizeBox_SlotRoot`, add `Overlay`, name it `Overlay_SlotContent`.
   - Horizontal Fill / Vertical Fill.

### Step S4 — Background State Border

1. Under `Overlay_SlotContent`, add `Border`, name it `Border_SlotBG`.
   - This is the primary visual state layer. Set Designer default to **empty slot** style:
     - Brush Tint: `#000000` A=46 (18%).
     - Border (use 1 px inner padding approach or set brush DrawAs=Box with border thickness 1): color `#FFFFFF` A=31 (12%).
   - Overlay Slot: Horizontal Fill / Vertical Fill.
   - Future read-only binding note: state styles are described in Section 12.4. Do not implement Blueprint binding logic in D4-B-Spec.

### Step S5 — Inner Detail Frame

1. Under `Overlay_SlotContent`, add `Border`, name it `Border_InnerFrame`.
   - This replicates D1's `::before` subtle inner border.
   - Overlay Slot Padding: 5 on all sides (insets by 5 px).
   - Brush Tint: Transparent (`#000000` A=0).
   - Border color: `#E9FBFF` A=15 (6%).
   - `Is Enabled` = false.

### Step S6 — Item Icon Token

1. Under `Overlay_SlotContent`, add `TextBlock`, name it `Text_SlotIcon`.
   - Overlay Slot: HAlign=Center, VAlign=Center.
   - Text: (empty string as default — filled from `ItemDisplayName` abbreviation or `ItemIcon` token)
   - Font Size: 20, Weight: Black, Color: `#E9FBFF`.
   - Future read-only binding note: text comes from snapshot, color from rarity palette (Section 4.3).

### Step S7 — Stack Count Badge

1. Under `Overlay_SlotContent`, add `Border`, name it `Border_StackCount`.
   - Overlay Slot: HAlign=Right, VAlign=Bottom, Padding: Right=4, Bottom=4.
   - Brush Tint: `#020A10` A=240 (94%).
   - Border color: `#E9FBFF` A=92 (36%).
   - Padding: Left=5, Right=5, Top=3, Bottom=3.
2. Inside `Border_StackCount`, add `TextBlock`, name `Text_StackCount`:
   - Text: `1` (placeholder).
   - Font Size: 13, Weight: Black, Color: `#E9FBFF`, Alignment: Center.
   - Min Desired Width: 22.
3. Future read-only binding note: visibility is Hidden when `!bHasItem`; text comes from `StackCount`.

### Step S8 — Locked Badge (Hidden by default)

1. Under `Overlay_SlotContent`, add `TextBlock`, name it `Text_LockedBadge`.
   - Overlay Slot: HAlign=Right, VAlign=Top, Padding: Right=6, Top=5.
   - Text: `LK` (placeholder glyph — replaced by lock icon asset in D4-F).
   - Font Size: 9, Weight: Black, Color: `#FFC02E`.
   - Visibility: **Hidden**.
2. Future read-only binding note: Visibility is Visible when `bIsLocked`.

### Step S9 — Tooltip Text (Hidden by default)

1. Under `Overlay_SlotContent`, add `Border`, name it `Border_Tooltip`.
   - Overlay Slot: HAlign=Center, VAlign=Bottom, Padding: Bottom=-24 (floats below slot edge).
   - Brush Tint: `#020A10` A=240 (94%).
   - Border color: `#20D9FF` A=107 (42%).
   - Padding: Left=8, Right=8, Top=3, Bottom=3.
   - Visibility: **Hidden**.
2. Inside `Border_Tooltip`, add `TextBlock` `Text_TooltipText`:
   - Text: `[Item Name]`, Font Size=11, Weight=Bold, Color=`#E9FBFF`.
3. Tooltip shown on hover in D4-C+.

### Step S10 — Save

Save `WBP_WTBRBagSlot` only.

---

## 8. WBP_WTBRCorpseLootPanel — Manual Designer Layout Steps

**File:** `Content/UI/HUD/WBP_WTBRCorpseLootPanel.uasset`  
**Role:** Left-adjacent corpse / container loot panel. Visible only when a loot context is active and
the bag layer is open. Contains header, scrollable entry list, empty-state label, and footer hint row.  
**D1 reference:** `.loot-panel`, `.loot-header`, `.loot-title`, `.loot-badge`, `.loot-list`,
`.loot-footer`, `.loot-hint-text`

### Step C1 — Open Widget

Open `WBP_WTBRCorpseLootPanel` in UMG Designer.

### Step C2 — Root SizeBox + Glass Border

1. Root: `Size Box`, name `SizeBox_LootRoot`.
   - Width Override: checked, value = **276**.
   - Max Desired Height: **660**.
2. Under `SizeBox_LootRoot`, add `Border`, name `Border_LootGlass`.
   - Brush Tint: `#02080E` A=189 (74%).
   - Border color: `#20D9FF` A=71 (28%).
   - Padding: 0 (padding is handled by child content rows).
   - Horizontal Fill / Vertical Fill.

### Step C3 — Inner Vertical Box

1. Under `Border_LootGlass`, add `Vertical Box`, name `VBox_LootContent`.

### Step C4 — Loot Header

1. Under `VBox_LootContent`, add `Border`, name `Border_LootHeaderBG`.
   - Brush Tint: `#20D9FF` A=14 (5.5%).
   - Bottom border: 1 px `#20D9FF` A=46 (18%) — simulate with inner border or separator below.
   - Padding: Left=14, Right=14, Top=10, Bottom=10.
2. Inside `Border_LootHeaderBG`, add `Horizontal Box` `HBox_LootHeader`:

   - **Title:** `TextBlock` `Text_LootTitle` — Text=`CORPSE LOOT`, Size=12, Weight=ExtraBold, Color=`#20D9FF`, Kerning=+4. HBox Slot Size Fill=1.0.

   - **Item count badge:**
     - `Size Box` (MinWidth=22, Height=20) → `Border` `Border_ItemCountBadge`:
       - Brush Tint: `#20D9FF` A=36 (14%).
       - Border: `#20D9FF` A=107 (42%).
       - Padding: Left=6, Right=6, Top=0, Bottom=0.
     - Inside: `TextBlock` `Text_LootItemCount` — Text=`6`, Size=11, Weight=ExtraBold, Color=`#20D9FF`, Alignment=Center.

3. Below `Border_LootHeaderBG`, add `Border` `Border_LootHeaderSep`:
   - Brush Tint: `#20D9FF` A=46 (18%). Height=1. Horizontal Fill.

### Step C5 — Loot Scroll List + Empty State

1. Under `VBox_LootContent`, add `Overlay`, name `Overlay_LootListArea`.
   - VBox Slot: Size Fill=1.0.

2. Under `Overlay_LootListArea`, add `Scroll Box`, name `ScrollBox_LootList`.
   - Overlay Slot: Horizontal Fill / Vertical Fill.
   - Orientation: Vertical.
   - Scrollbar Visibility: Auto (auto-hide when not needed).
   - Content padding: Left=6, Right=6, Top=6, Bottom=6.
   - (Scrollbar thickness 3 px requires a Scrollbar Style override referencing a slim style asset — defer to D4-F if custom scrollbar style is not available.)

3. Under `ScrollBox_LootList`, add **4–6 designer-preview** `WBP_WTBRCorpseLootEntry` widgets.
   - Each entry HBox Slot Padding: Bottom=3 (gap between rows).
   - At runtime these are replaced by dynamic population from `CorpseLootEntries[]`.

4. Under `Overlay_LootListArea`, add `TextBlock` `Text_LootEmpty`.
   - Overlay Slot: HAlign=Center, VAlign=Center.
   - Text: `NO ITEMS`, Size=12, Weight=Bold, Color=`#8AB1BA`, Alignment=Center.
   - Visibility: **Hidden** (shown when `CorpseLootItemCount == 0` — see Section 14).

### Step C6 — Loot Footer

1. Under `VBox_LootContent`, add `Border` `Border_LootFooter`.
   - Border top: 1 px `#FFFFFF` A=18 (7%) — use same separator technique as Step C4.
   - Brush Tint: Transparent.
   - Padding: Left=14, Right=14, Top=8, Bottom=8.
2. Inside `Border_LootFooter`, add `TextBlock` `Text_LootHint`:
   - Text: `[Input Action] to take item` (placeholder — DO NOT hardcode a key label).
   - Size=11, Color=`#8AB1BA`, Kerning=+3, Alignment=Center, Opacity=72%.
3. Future read-only binding note: text comes from `CorpseLootFooterHintText` snapshot field (pre-resolved by ViewModel — see Section 12).

### Step C7 — Default Visibility

1. Select `SizeBox_LootRoot`.
2. Set Visibility = **Hidden**.
3. Future read-only binding note: visibility toggles via `bIsCorpseLootVisible`.

### Step C8 — Save

Save `WBP_WTBRCorpseLootPanel` only.

---

## 9. WBP_WTBRCorpseLootEntry — Manual Designer Layout Steps

**File:** `Content/UI/HUD/WBP_WTBRCorpseLootEntry.uasset`  
**Role:** Individual row entry in the Corpse Loot scroll list. Leaf widget. Displays item icon token,
display name, type/rarity meta, and quantity. Visual state driven by read-only snapshot flags.  
**D1 reference:** `.loot-item`, `.loot-icon-cell`, `.loot-info`, `.loot-name`, `.loot-meta`,
`.loot-qty`, `.loot-item--hover`, `.loot-item--selected`

### Step E1 — Open Widget

Open `WBP_WTBRCorpseLootEntry` in UMG Designer.

### Step E2 — Root Border (Row Background + State Layer)

1. Root: `Border`, name `Border_EntryRoot`.
2. Default (normal state) style:
   - Brush Tint: `#FFFFFF` A=6 (2.5%).
   - Border color: `#FFFFFF` A=15 (6%).
3. Padding: Left=8, Right=10, Top=6, Bottom=6.
4. Min Desired Height: 52.
5. Horizontal Fill.
6. Future read-only binding note: state styles (hover / selected) are described in Section 12.6 and Section 9 Step E7.

### Step E3 — Inner Horizontal Box

1. Under `Border_EntryRoot`, add `Horizontal Box` `HBox_EntryRow`.
   - Children vertically centered (VAlign=Center on HBox).

### Step E4 — Icon Cell

1. Under `HBox_EntryRow`, add `Size Box` → `Border` `Border_EntryIcon`.
   - SizeBox: Width=36, Height=36.
   - Border Brush Tint: `#FFFFFF` A=10 (4%).
   - Border color: `#FFFFFF` A=26 (10%).
   - Padding: 0.
   - HBox Slot Padding: Right=8.
2. Inside `Border_EntryIcon`, add `TextBlock` `Text_EntryIconToken`:
   - Text: `??` (placeholder 2-char token).
   - Size=13, Weight=Black, Color=`#8AB1BA` (future read-only binding may override by rarity color).
   - Alignment: Center / Center.

### Step E5 — Item Info Column

1. Under `HBox_EntryRow`, add `Vertical Box` `VBox_EntryInfo`.
   - HBox Slot: Size Fill=1.0. Min Width=0.
2. Under `VBox_EntryInfo`:
   - `TextBlock` `Text_EntryName`:
     - Text: `[Item Name]`, Size=13, Weight=Bold, Color=`#E9FBFF`.
     - Clipping: Clip To Bounds.
   - `TextBlock` `Text_EntryMeta`:
     - Text: `[Type] | [Rarity]`, Size=10, Color=`#8AB1BA`, Kerning=+1.
     - VBox Slot Padding: Top=2.

### Step E6 — Quantity Label

1. Under `HBox_EntryRow`, add `TextBlock` `Text_EntryQty`.
   - Text: `x1` (placeholder).
   - Size=14, Weight=ExtraBold, Color=`#E9FBFF`.
   - HBox Slot: HAlign=Right, VAlign=Center.

### Step E7 — Hover / Selected State Reference

State styles are future read-only binding references. Set Designer preview to Normal and do not implement Blueprint binding logic in D4-B-Spec.

| State | `Border_EntryRoot` Brush Tint | `Border_EntryRoot` Border Color |
|-------|-------------------------------|--------------------------------|
| Normal | `#FFFFFF` A=6 | `#FFFFFF` A=15 |
| Hover | `#20D9FF` A=20 (8%) | `#20D9FF` A=97 (38%) |
| Selected | `#20D9FF` A=31 (12%) | `#20D9FF` A=255 (full) |
| Blocked transfer | Keep row style; `Text_EntryQty` color → `#8AB1BA` to signal unavailable |

### Step E8 — Save

Save `WBP_WTBRCorpseLootEntry` only.

---

## 10. Naming Rules for Widgets

### 10.1 Prefix by Widget Type

| UMG Widget Type | Prefix |
|-----------------|--------|
| Canvas Panel | `Canvas_` |
| Overlay | `Overlay_` |
| Border | `Border_` |
| Vertical Box | `VBox_` |
| Horizontal Box | `HBox_` |
| Size Box | `SizeBox_` |
| Scroll Box | `ScrollBox_` |
| Uniform Grid Panel | `Grid_` |
| Text Block | `Text_` |
| Image | `Image_` |
| Progress Bar | `ProgressBar_` |
| Button | `Btn_` |
| Spacer | `Spacer_` |
| Custom WBP child | Use the WBP asset name (e.g., `WBP_WTBRBagSlot`) |

### 10.2 Role Suffix Convention

After the prefix, append a short descriptive role in PascalCase:

```
Border_GlassFrame        ← glass background border
Text_BagTitle            ← bag panel title label
HBox_LootHeader          ← header row in loot panel
Grid_BagInventory        ← inventory grid
ProgressBar_Volume       ← volume capacity bar
Overlay_LayerRoot        ← root overlay for layer widget
```

### 10.3 Prohibited Widget Names

- Default auto-generated names (e.g., `TextBlock_0`, `Border_3`)
- Single-letter names or opaque abbreviations without context
- Names containing physical key labels (`Btn_FKey`, `Text_PressE`, etc.)

---

## 11. Placeholder Text Rules

Placeholder text is for Designer preview only. At runtime all fields are replaced by bound data.

### 11.1 Rules

| Rule | Detail |
|------|--------|
| No hardcoded item names | Use `[Item Name]`, `[Trigger Name]` |
| No hardcoded key labels | Use `[Input Action]` or `[Binding]` — not `F`, `X`, `Q`, `E`, `LMB`, `RMB`, `4`, or any physical key |
| No hardcoded quantities | Use `1` or `x1` as neutral placeholder |
| Bracket notation | Wrap all unbound runtime strings in `[...]` |
| No hardcoded rarity strings | Use `[Rarity]` |

### 11.2 Approved Placeholder Strings by Field

| Widget field | Approved Designer placeholder |
|-------------|-------------------------------|
| Bag title | `BAG` (fixed label — not a placeholder) |
| Bag glyph | `BG` (text token — replaced by icon in D4-F) |
| Slot count | `12 / 14 SLOTS` |
| Volume count | `48 / 60 VOL` |
| Volume bar value | `48 / 60` |
| Main trigger name | `[Trigger Name]` |
| Main trigger sub | `Blade | Slot M1` |
| Sub trigger name | `[Trigger Name]` |
| Sub trigger sub | `Blade | Slot S1` |
| Main slot badge | `M1` |
| Sub slot badge | `S1` |
| Reserve slot index | `M2`, `M3`, `M4`, `S2`, `S3`, `S4` |
| Reserve slot name (filled) | `[Name]` |
| Reserve slot name (empty) | `RES` |
| Loot panel title | `CORPSE LOOT` (fixed label — not a placeholder) |
| Loot item count badge | `6` |
| Loot entry name | `[Item Name]` |
| Loot entry type/rarity meta | `[Type] | [Rarity]` |
| Loot entry quantity | `x1` |
| Loot entry icon token | `??` (or 2-char abbreviation of item type as hint) |
| Footer hint text | `[Input Action] to take item` — DO NOT use a physical key label |
| Empty loot label | `NO ITEMS` (fixed fallback label) |
| Bag full warning | `BAG FULL / CANNOT TRANSFER` (fixed warning label) |
| Close button glyph | `✕` (visual glyph — not a key label) |
| Locked badge | `LK` (placeholder — replaced by icon art in D4-F) |
| Bag slot icon token | (empty string default; abbreviation set by binding at runtime) |
| Bag slot count | `1` |

---

## 12. Future Data / Binding Notes — Read-Only ViewModel / Snapshot Only

This section documents future read-only binding targets only. It does not authorize creating or editing Blueprint binding logic in D4-B-Spec. Any future binding is **read-only** and must not write to gameplay state.

### 12.1 Binding Source

All fields are read from the HUD ViewModel / read-only snapshot data exposed by
`UWTBRHUDViewModelComponent` (or equivalent UI contract component). The ViewModel derives
its values from replicated / authoritative state. Widgets never access gameplay objects directly.

### 12.2 WBP_WTBRBagLootLayer Future Binding References

| Widget | Property | Source field | Notes |
|--------|----------|-------------|-------|
| `Overlay_LayerRoot` | Visibility | `bIsBagLayerActive` | Visible = true, Hidden = false |

### 12.3 WBP_WTBRBagPanel Future Binding References

| Widget | Property | Source field | Notes |
|--------|----------|-------------|-------|
| `Text_SlotCur` | Text | `CurrentSlotCount` | Int → string |
| `Text_SlotMax` | Text | `MaxSlotCount` | Int → string |
| `Text_VolCur` | Text | `CurrentVolume` | Int → string |
| `Text_VolMax` | Text | `MaxVolume` | Int → string |
| `Text_VolBarValue` | Text | `CurrentVolume` + `/` + `MaxVolume` | Formatted string |
| `ProgressBar_Volume` | Percent | `CurrentVolume / MaxVolume` | Float 0–1 |
| `ProgressBar_Volume` | Fill Color | `bShowBagFullWarning` | Normal: `#20D9FF`; Warning: `#FFC02E` |
| `HBox_BagFullWarn` | Visibility | `bShowBagFullWarning` | Visible when true |
| `Text_MainTriggerName` | Text | `MainTriggerName` | String |
| `Text_MainTriggerSub` | Text | `MainTriggerSlotLabel` | String (e.g., `Blade | Slot M1`) |
| `Text_MainTriggerIconToken` | Text | First 2 chars of `MainTriggerName` | Abbreviation token |
| `Text_SubTriggerName` | Text | `SubTriggerName` | String |
| `Text_SubTriggerSub` | Text | `SubTriggerSlotLabel` | String |
| `Text_SubTriggerIconToken` | Text | First 2 chars of `SubTriggerName` | Abbreviation token |
| Reserve slot border + labels | Color + Text | `MainReserveEntries[]`, `SubReserveEntries[]` | Per-entry `bIsFilled`, `DisplayName`, `SlotLabel` |

### 12.4 WBP_WTBRBagSlot Future Binding References

Driven by per-slot snapshot `InventorySlots[i]`. Resolve a single state enum from priority order below,
then apply the corresponding brush style to `Border_SlotBG`.

**State priority (top wins):**

| Priority | Condition | `Border_SlotBG` Tint | `Border_SlotBG` Border |
|----------|-----------|---------------------|----------------------|
| 1 | `bIsReservedVisualCell` | `#000000` A=77 (30%) | `#8AB1BA` A=56, dashed |
| 2 | `bIsLocked` | `#000000` A=87 + hatch pattern | `#FFFFFF` A=41 |
| 3 | `bIsDragTarget` | `#20D9FF` A=36 (14%) | `#20D9FF` A=255 + pulse anim |
| 4 | `bIsCannotTransfer` | `#FFC02E` A=20 + cross pattern | `#FFC02E` A=189 |
| 5 | `bIsSelected` | `#20D9FF` A=41 (16%) | `#20D9FF` A=255 |
| 6 | `bIsHovered` | `#20D9FF` A=28 (11%) | `#20D9FF` A=184 (72%) |
| 7 | `bHasItem` | `#FFFFFF` A=14 (5.5%) gradient | `#E9FBFF` A=46 (18%) |
| 8 | Default (empty) | `#000000` A=46 (18%) | `#FFFFFF` A=31 (12%), dashed |

Additional per-slot bindings:

| Widget | Property | Source field | Notes |
|--------|----------|-------------|-------|
| `Text_SlotIcon` | Text | `ItemDisplayName` abbreviation or icon token | 2-char token |
| `Text_SlotIcon` | Color And Opacity | `Rarity` | Map to rarity palette (Section 4.3) |
| `Border_StackCount` | Visibility | `bHasItem` | Hidden when slot is empty |
| `Text_StackCount` | Text | `StackCount` | Int → string |
| `Text_LockedBadge` | Visibility | `bIsLocked` | Visible when true |

### 12.5 WBP_WTBRCorpseLootPanel Future Binding References

| Widget | Property | Source field | Notes |
|--------|----------|-------------|-------|
| `SizeBox_LootRoot` | Visibility | `bIsCorpseLootVisible` | Visible when `bIsBagLayerActive && bHasLootContext` |
| `Text_LootTitle` | Text | `CorpseLootTitle` | String |
| `Text_LootItemCount` | Text | `CorpseLootItemCount` | Int → string |
| `ScrollBox_LootList` | Visibility | `CorpseLootItemCount > 0` | Visible when entries present |
| `Text_LootEmpty` | Visibility | `CorpseLootItemCount == 0` | Visible when no entries |
| `Text_LootHint` | Text | `CorpseLootFooterHintText` | Pre-resolved string from ViewModel — not a raw key label |

### 12.6 WBP_WTBRCorpseLootEntry Future Binding References

Driven by per-entry snapshot `CorpseLootEntries[i]`:

| Widget | Property | Source field | Notes |
|--------|----------|-------------|-------|
| `Text_EntryIconToken` | Text | `DisplayName` first 2 chars or icon token | Abbreviation |
| `Text_EntryIconToken` | Color | `Rarity` | Map to rarity palette |
| `Text_EntryName` | Text | `DisplayName` | String |
| `Text_EntryMeta` | Text | `EntryTypeLabel + " | " + Rarity string` | Combine from snapshot |
| `Text_EntryQty` | Text | `"x" + Quantity` | Int → prefixed string |
| `Border_EntryRoot` | Brush Tint + Border Color | `bIsSelected`, `bIsHovered` | See Section 9, Step E7 |
| `Text_EntryQty` | Color | `bCanTransfer` | Normal: `#E9FBFF`; Blocked: `#8AB1BA` |

---

## 13. Request-Only Action Notes

### 13.1 Golden Rule

> **Client UI never decides success. Server validates and mutates. UI refreshes from authoritative replicated / read-only state.**

### 13.2 What D4-B Does NOT Wire

D4-B covers Designer layout only. Read-only binding fields are documented as future references. The following actions are **not wired** in D4-B:

| Action | Future pass | Existing function reference |
|--------|------------|----------------------------|
| Close bag / cancel UI | D4-C | `UWTBRHUDViewModelComponent::RequestCancelCurrentUIOrAction()` |
| Select bag slot | D4-C | Request select slot dispatch |
| Select loot entry | D4-C | Request select entry dispatch |
| Take loot item | D4-C | `AWTBRCharacter::Server_RequestPickupCorpseLootEntry(...)` |
| Transfer loot to bag slot | D4-C | Server-validated request with proposed slot hint |
| Drop bag slot | D4-C | Server-validated request |
| Move / swap slots | D4-C | Server-validated request |
| Use item | D4-C | Server-validated request |
| Drag/drop target highlight | D4-D | Visual-only, no authoritative action |

### 13.3 Forbidden UI Mutations (All Passes)

UI must **never** directly:

- Remove an item from corpse loot
- Add an item to inventory / bag
- Swap trigger slots
- Consume or mutate ground items
- Mutate inventory, slots, loot entries, corpse container state, ground item state, Vael (Trion energy), HP, cooldowns, or trigger state
- Apply any authoritative game state change without going through a server-validated request path

---

## 14. Visibility States

### 14.1 WBP_WTBRBagLootLayer

| Condition | `Overlay_LayerRoot` Visibility |
|-----------|-------------------------------|
| Bag closed — gameplay combat | `Hidden` |
| Bag open | `Visible` |

### 14.2 WBP_WTBRCorpseLootPanel

| Condition | `SizeBox_LootRoot` Visibility |
|-----------|------------------------------|
| No loot context, or bag closed | `Hidden` |
| Bag open **and** loot context present | `Visible` |

### 14.3 WBP_WTBRBagPanel — Sub-States

| Condition | Widget | State |
|-----------|--------|-------|
| Default | `HBox_BagFullWarn` | `Hidden` |
| `bShowBagFullWarning = true` | `HBox_BagFullWarn` | `Visible` (alternate state — not persistent default) |
| Default | `ProgressBar_Volume` fill | Cyan (`#20D9FF`) |
| `bShowBagFullWarning = true` | `ProgressBar_Volume` fill | Yellow (`#FFC02E`) |

### 14.4 WBP_WTBRCorpseLootPanel — Sub-States

| Condition | `ScrollBox_LootList` | `Text_LootEmpty` |
|-----------|---------------------|-----------------|
| `CorpseLootItemCount > 0` | `Visible` | `Hidden` |
| `CorpseLootItemCount == 0` | `Hidden` | `Visible` |

### 14.5 WBP_WTBRBagSlot — Visual States (Section 12.4 priority order)

| State flag | Visual treatment |
|------------|-----------------|
| `bIsReservedVisualCell` | Reserved dark dashed cell |
| `bIsLocked` | Hatch pattern, locked badge visible |
| `bIsDragTarget` | Cyan border full + pulse animation |
| `bIsCannotTransfer` | Yellow/cross pattern border |
| `bIsSelected` | Cyan glow, thick cyan border |
| `bIsHovered` | Lighter cyan tint, partial cyan border |
| `bHasItem` | Filled dark-glass slot with item token |
| Default | Empty dashed slot |
| Parent `bShowBagFullWarning` | Alternate state — affects parent footer only, not individual slots |

### 14.6 WBP_WTBRCorpseLootEntry — States

| State flag | `Border_EntryRoot` treatment |
|------------|------------------------------|
| Default (normal) | `#FFFFFF` A=6 tint; `#FFFFFF` A=15 border |
| `bIsHovered` | `#20D9FF` A=20 tint; `#20D9FF` A=97 border |
| `bIsSelected` | `#20D9FF` A=31 tint; `#20D9FF` A=255 border |
| `!bCanTransfer` | Row style unchanged; `Text_EntryQty` color dimmed to `#8AB1BA` |

---

## 15. Manual UE Save Checklist

Follow this checklist **every time** you save during D4-B work. Do not deviate.

```
[ ] Open only ONE WBP file at a time in UMG Designer.
[ ] After completing layout changes, save ONLY that file:
      File → Save Current Asset   (Ctrl+S on the Designer tab)
      OR right-click asset in Content Browser → Save.
[ ] Verify the save confirmation shows the correct WBP name.
[ ] Do NOT use File → Save All (Ctrl+Shift+S).
[ ] Do NOT save ThirdPersonMap or any .umap file.
[ ] Do NOT save any Content/__ExternalActors__/ file.
[ ] Do NOT save Content/UI/Sandbox/WBP_WTBRToolingSpike_Test.
[ ] Do NOT save Content/UI/Sandbox/WBP_WTBRBagSlot_CloneSpike.
[ ] After saving, check git status in terminal to confirm only
      the 5 approved .uasset files show as modified.
[ ] If any unexpected file appears modified in git status, do NOT stage it.
```

**Approved saves in D4-B:**

| File | Approved to save |
|------|-----------------|
| `Content/UI/HUD/WBP_WTBRBagLootLayer.uasset` | Yes |
| `Content/UI/HUD/WBP_WTBRBagPanel.uasset` | Yes |
| `Content/UI/HUD/WBP_WTBRBagSlot.uasset` | Yes |
| `Content/UI/HUD/WBP_WTBRCorpseLootPanel.uasset` | Yes |
| `Content/UI/HUD/WBP_WTBRCorpseLootEntry.uasset` | Yes |
| Any other file | **No** |

---

## 16. Post-Pass Validation Checklist

After all Designer work and saves are complete for D4-B:

```
Layout verification:
[ ] WBP_WTBRBagLootLayer:
    Canvas_LayerRoot → Overlay_LayerRoot → [ Border_ContextDim, HBox_PanelsGroup ]
    HBox_PanelsGroup children: WBP_WTBRCorpseLootPanel (left), WBP_WTBRBagPanel (right)
    Overlay_LayerRoot default Visibility = Hidden.

[ ] WBP_WTBRBagPanel:
    SizeBox width = 356. VBox_BagContent contains all sections in order:
    HBox_BagHeader → Border_HeaderSep → Border_CapRowBG →
    VBox_ActiveMain → VBox_ActiveSub → VBox_ReserveMain → VBox_ReserveSub →
    Border_InvSep → Text_InvLabel → Grid_BagInventory → Spacer_FooterPush →
    Border_FooterSep → VBox_BagFooter.
    Grid_BagInventory has 14x WBP_WTBRBagSlot + 2x reserved Border cells = 16 total.
    HBox_BagFullWarn default Visibility = Hidden.

[ ] WBP_WTBRBagSlot:
    SizeBox_SlotRoot → Overlay_SlotContent contains:
    Border_SlotBG (fill), Border_InnerFrame (inset 5px), Text_SlotIcon (center),
    Border_StackCount (bottom-right), Text_LockedBadge (top-right, Hidden),
    Border_Tooltip (Hidden).

[ ] WBP_WTBRCorpseLootPanel:
    SizeBox width = 276, MaxDesiredHeight = 660. VBox_LootContent contains:
    Border_LootHeaderBG → Border_LootHeaderSep → Overlay_LootListArea →
    Border_LootFooter.
    Overlay_LootListArea has ScrollBox_LootList + Text_LootEmpty (Hidden).
    SizeBox_LootRoot default Visibility = Hidden.

[ ] WBP_WTBRCorpseLootEntry:
    Border_EntryRoot (min-height 52) → HBox_EntryRow →
    [ Border_EntryIcon (36x36), VBox_EntryInfo (fill), Text_EntryQty ]

Style verification:
[ ] No panel has a fully opaque white or solid bright background.
[ ] Main accent color is cyan (#20D9FF or close sRGB match).
[ ] Sub-trigger / sub-slot accents are orange (#FF4B2F or close sRGB match).
[ ] Text on dark panels is near-white (#E9FBFF or #FFFFFF) and readable.
[ ] HBox_BagFullWarn is Hidden in default state.
[ ] Text_LootEmpty is Hidden when preview entries are present.
[ ] No TextBlock contains a physical key label (F, X, Q, E, LMB, RMB, 4).

Future binding reference review:
[ ] ProgressBar_Volume has a documented future source of CurrentVolume / MaxVolume (read-only float 0–1).
[ ] HBox_BagFullWarn has a documented future source of bShowBagFullWarning (read-only bool).
[ ] SizeBox_LootRoot has a documented future source of bIsCorpseLootVisible (read-only bool).
[ ] Overlay_LayerRoot has a documented future source of bIsBagLayerActive (read-only bool).
[ ] No Designer work in D4-B-Spec creates a writable/output path to a gameplay variable.

Guardrail verification:
[ ] Event Graph is empty in all 5 WBPs (zero Blueprint nodes added in D4-B).
[ ] No drag/drop event handlers (OnDragDetected, OnDrop) in any WBP.
[ ] No Server_ RPC call references in any Blueprint graph.
[ ] git status shows ONLY the 5 approved .uasset files as modified (status M).
[ ] ThirdPersonMap is NOT listed in git status modified files.
[ ] No .log, .diff, .claude, or other non-asset file was staged.
```

---

## 17. Explicit Non-Goals for D4-B

| Non-goal | Detail |
|----------|--------|
| No Event Graph | Do not add any Blueprint node in any WBP. Designer layout and read-only binding notes only. |
| No gameplay mutation | No Blueprint node may write to inventory, HP, Vael, trigger slots, or any authoritative state. |
| No drag/drop logic | Do not create `OnDragDetected`, `OnDrop`, drag visual proxy widgets, or drag/drop event bindings. |
| No server request integration | No RPC wiring, no `Server_` / `Multicast_` call dispatch, no `RequestPickupFocusedTarget` wiring. |
| No generated WBP changes | Do not modify `WBP_WTBRToolingSpike_Test` or `WBP_WTBRBagSlot_CloneSpike` (sandbox spike assets). |
| No build required | D4-B-Spec is docs-only. No build is required when only this Markdown file changes. Future Designer layout work does not require C++ compilation unless a code file is added. |
| No art asset import | Do not import textures, icons, sprites, sounds, or VFX. Text-token placeholders only in D4-B. |
| No staging or commit | Do not `git add` or `git commit` without a separate human approval pass on the modified `.uasset` files. |
| No Save All | Do not use File → Save All at any point during D4-B work. |
| No map edits | Do not open, modify, or save ThirdPersonMap or any `.umap` file. |
