# UMGJsonGenerator Web Preview

Phase 2 MVP adds a standalone browser preview for UMGJsonGenerator JSON files.

The tool is a visual reference step before importing JSON into Unreal Engine. It does not create `.uasset` files, does not run inside Unreal, and does not replace the Unreal importer.

## How to Open

Open this file directly in a browser:

`Plugins/UMGJsonGenerator/WebPreview/index.html`

No install step is required. There is no React, Angular, Vite, npm, server, or build process.

## How to Load JSON

Use one of the input methods in the left panel:

1. Pick a `.json` file with the file picker.
2. Drag and drop a `.json` file onto the drop zone.
3. Paste raw JSON into the textarea, then press `Load / Render`.

Because this runs from a local `index.html`, the preview does not use `fetch()` to auto-load local examples.

## Supported Widget Preview Types

The preview approximates the same widget types supported by the UE5.1 plugin:

- `CanvasPanel`
- `TextBlock`
- `Button`
- `Image`
- `Border`
- `VerticalBox`
- `HorizontalBox`
- `ProgressBar`
- `ScrollBox`
- `Overlay`
- `SizeBox`
- `Spacer`

## Validation Coverage

The browser validation is intentionally similar to the importer validation. It reports errors or warnings for:

- invalid JSON syntax
- missing `widgetName`
- empty `widgetName`
- missing `root`
- root type not `CanvasPanel`
- duplicate widget names
- missing parent
- parent not found
- unsupported widget type
- invalid color hex
- invalid `justify`
- invalid `verticalAlign`
- invalid `drawAs`
- missing `position` or `size` for CanvasPanel children

Bad JSON should never crash the browser preview. It should show clear feedback in the Validation panel.

## How This Fits the Unreal Import Workflow

1. Design or generate a UMGJsonGenerator JSON file.
2. Open the Web Preview and visually inspect layout, hierarchy, stats, warnings, and skipped widgets.
3. Fix JSON issues while still outside Unreal.
4. Import the JSON with the Unreal Editor plugin.
5. Treat the Unreal importer and generated WBP as the source of truth.

An HTML/CSS-to-JSON converter now lives alongside this preview — see the Converter section below.

## HTML → JSON Converter (`converter.html`)

`converter.html` turns a rendered HTML/CSS prototype into UMGJsonGenerator JSON automatically,
replacing the manual browser-measurement workflow used in the Phase 3B.1 fidelity pass.

How to run:

1. Start a static server at the **repo root** (converter and prototype must be same-origin):
   `python -m http.server 8743` from `WTBR-Vaelborne/`.
2. Open `http://localhost:8743/Plugins/UMGJsonGenerator/WebPreview/converter.html`.
3. Enter the prototype URL (e.g. `/Docs/UI_Prototypes/Vaelborne_InMatch_HUD.html`), the capture
   root selector (e.g. `.hud-canvas`), `widgetName`, and an optional `parentClass`.
4. Press **1. Load page**, then **2. Convert**, then download or copy the JSON.
5. Validate the result in `index.html`, then import in Unreal.

What it does:

- Measures the **rendered** DOM with `getBoundingClientRect()` / `getComputedStyle()` at the
  target resolution; ancestor transform scaling (fit-to-window stages) is neutralized.
- Emits flat `RootCanvas` children with absolute position/size/zOrder (paint order), hex+alpha
  colors (element opacity folded in), font sizes, and text justification.
- Auto-classifies: `<img>` → Image, `<button>`/`role=button` → Button, `bar|progress|meter`
  class + background → ProgressBar (percent measured from the fill child), visible
  background/border → Border, leaf/direct text nodes → TextBlock (Range-measured for mixed
  content). Names follow the project prefixes (`Txt_`/`Border_`/`Img_`/`PB_`/`Btn_`).
- Override hooks in the prototype HTML: `data-umg="skip"` (drop subtree), `data-umg="ignore"`
  (descend only), `data-umg="keep"` (keep an `aria-hidden` subtree), `data-umg-type`,
  `data-umg-name`.

Converter limitations:

- Pseudo-elements (`::before`/`::after`), gradients, box-shadow, clip-path, and blur are not
  convertible — flat approximations plus a warning are emitted where possible.
- `<img>`/background images emit an Image widget without `imagePath`; assign UE texture paths
  manually.
- Decorative `aria-hidden` art (skylines, rain, beams) is skipped by default via the checkbox.

## Manual Test Checklist

Open `index.html`, then load:

- `Plugins/UMGJsonGenerator/Examples/WBP_MainMenu.json`
- `Plugins/UMGJsonGenerator/Examples/WBP_AllSupportedWidgets_Test.json`
- at least one file from `Plugins/UMGJsonGenerator/Examples/Invalid/*.json`

Expected results:

- Valid files render a scaled visual preview.
- Widget hierarchy is shown in the right panel.
- Import stats are shown.
- Invalid files show warnings or errors without crashing.
- Missing or skipped widgets are called out in the Validation panel.

## Known Limitations

- Rendering is approximate HTML/CSS, not pixel-identical UMG.
- Texture paths are shown as labeled placeholders; local Unreal assets are not loaded.
- Anchors and alignment are only lightly approximated.
- Blueprint logic, bindings, drag/drop logic, and gameplay behavior are not represented.
- The Unreal importer remains the authority for actual generated WBP output.
