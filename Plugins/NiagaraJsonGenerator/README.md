# WTBR Niagara JSON Generator (v1.1.0)

**Engine:** Unreal Engine 5.1.1
**Type:** Editor-only plugin (C++)
**Status:** Production template-driven importer for WTBR.

Version 1 keeps the safe template workflow and adds:

- full read-only validation (`WTBR.Niagara.ValidateJson`);
- strict template contracts that abort before creating/updating an output asset;
- batch import/validation manifests (`WTBR.Niagara.ImportBatch`);
- schema versioning while remaining compatible with v0.1 JSON;
- `vector2` and `vector4` parameters in addition to the original types;
- three Tools-menu actions for import, validation, and batch workflows.
- a **Generate VFX Preset** dialog for Color, Energy, Speed, and Spark Count;
- automatic Niagara Editor preview after a preset is generated;
- runtime asset-parameter overrides for Material, Texture, Curve, Mesh, and
  Ribbon-profile assets, bound through Trigger Data Assets.

---

## Production Scope (intentional limits)

| In scope | Deliberate non-goals |
|----------|------------------------------------------------------|
| Duplicate an existing Niagara System template asset | Creating a Niagara System / emitter graph from scratch |
| Set exposed **User Parameter** defaults on the asset | Touching module stack, scratch modules, or any graph internals |
| Save the asset so values persist across editor restarts | Runtime component parameter overrides (already possible via BP) |
| Update-in-place when the output asset already exists | Replacing artistic review or performance profiling |
| Validate one spec or batch-import many specs | Automatic authoring of arbitrary Niagara module graphs |

---

## One-Time Template Setup (manual, in Unreal Editor) — REQUIRED for production validation

The production template lives at `/Game/VFX/Templates/NS_Template_Burst` and must
expose 4 User Parameters **wired so they drive the visual**. ~10 minutes, once.
This is deliberately human work: programmatic module-stack editing is out of scope
by design (see Production Scope).

1. Content Browser → `Content/VFX/` → create folder `Templates`.
2. Right-click in it → **FX → Niagara System** → **New system from selected emitter(s)**
   → select the **Fountain** emitter template → Finish → name the asset `NS_Template_Burst`.
3. Open it. Turn the Fountain into a one-shot burst:
   - Select the **Fountain** emitter's **Emitter Update** group.
   - Delete the **Spawn Rate** module.
   - Click **+** on Emitter Update → Spawning → **Spawn Burst Instantaneous**.
   - In **Emitter State**: Life Cycle Mode = `Self`, Loop Behavior = **`Infinite`**,
     Loop Duration = `1.0` — the burst then pulses once per second, which makes
     visual changes after re-import easy to see in a level. (Switch to `Once` when
     using it as a real gameplay template later.)
4. In the **User Parameters** panel (left side; **Window → User Parameters** if hidden),
   click **+** and add — names must match exactly, no `User.` prefix when typing
   (Niagara adds the namespace itself):
   - `Color` — type **Linear Color** (default e.g. cyan `0.13, 0.85, 1.0, 1.0`)
   - `Lifetime` — type **float** (default `1.0`)
   - `Size` — type **float** (default `12.0`)
   - `SpawnCount` — type **int32** (default `50`)
5. Wire each into the stack. On each module input below, click the **▾ dropdown**
   on the right of the value box → **Link Inputs → User → <param>**:
   - **Spawn Burst Instantaneous → Spawn Count** → link `SpawnCount`
   - **Initialize Particle → Point Attributes → Lifetime** → link `Lifetime`
     (if Lifetime Mode is `Random`, switch it to `Direct Set` first so there is a
     single linkable input)
   - **Initialize Particle → Point Attributes → Color** → link `Color`
   - **Initialize Particle → Sprite Attributes → Sprite Size** → set Sprite Size
     Mode = `Uniform`, then link **Uniform Sprite Size** → `Size`
6. Sanity-check in the template itself: change each value in the User Parameters
   panel and confirm the preview reacts (count, size, tint, how long particles
   live). Undo your test edits (or re-enter the defaults from step 4) — the
   template's saved defaults are what every generated variant starts from.
7. Save. Done — you never need to touch this asset again for the validation loop.

---

## How to Run

**Method A — Tools menu (interactive):**
1. Open the project in Unreal Editor (plugin builds with the WTBREditor target).
2. **Tools → Import Niagara JSON...**
3. Select `Plugins/NiagaraJsonGenerator/Examples/NS_Test_Burst_FromJson.json`.
4. A toast shows the summary; details are in the Output Log under `LogNiagaraJsonSpike`.

**Method B — Console command:**
```
WTBR.Niagara.ImportJson E:\World Trigger\WTBR-Vaelborne\Plugins\NiagaraJsonGenerator\Examples\NS_Test_Burst_FromJson.json
```

**Validate without modifying assets:**

```text
WTBR.Niagara.ValidateJson E:\Path\To\Effect.json
```

Validation checks JSON shape, the template asset, every exposed parameter name/type,
and the output target. It never duplicates, modifies, dirties, or saves an asset.

**Batch import or validation:**

```text
WTBR.Niagara.ImportBatch E:\Path\To\Batch.json
```

Batch manifest schema (relative spec paths resolve beside the manifest):

```json
{
  "schemaVersion": 1,
  "validateOnly": false,
  "stopOnError": false,
  "files": [
    "NS_Telorn_Impact.json",
    "NS_Piercex_Impact.json",
    "NS_Fulgris_Impact.json"
  ]
}
```

**Bind generated Sniper VFX to WTBR gameplay:**

```text
WTBR.Niagara.AutoBindSniper E:\Path\To\SniperVFX.bind.json
```

The binder writes the selected trail, default impact, surface overrides, and
per-client impact cull distance into Telorn, Piercex, and Fulgris Trigger Data
Assets. It enables the runtime built-in impact path, so migrated projectiles no
longer need a Blueprint `Spawn System at Location` graph.

```json
{
  "schemaVersion": 1,
  "bindings": [
    {
      "sniper": "Telorn",
      "dataAsset": "/Game/Data/Triggers/DA_Telorn",
      "trail": "/Game/VFX/Sniper/NS_Telorn_Trail",
      "impact": "/Game/VFX/Sniper/NS_Telorn_Impact",
      "maxImpactVFXDistance": 20000,
      "surfaceOverrides": [
        { "surfaceType": 1, "effect": "/Game/VFX/Sniper/NS_Telorn_Impact_Metal" }
      ],
      "assetOverrides": [
        { "parameter": "User.ImpactMaterial", "asset": "/Game/VFX/Materials/M_Impact_Metal" },
        { "parameter": "User.RibbonProfile", "asset": "/Game/VFX/Curves/CV_RibbonProfile" }
      ]
    }
  ]
}
```

`surfaceType` is the numeric physical-surface ID configured in **Project Settings
→ Physics → Physical Surfaces**. Omit `surfaceOverrides` to use `impact` for every
surface. The supplied WTBR Sniper manifest deliberately does this until dedicated
metal/stone/shield impact assets exist.

`assetOverrides` is optional. Each item binds an existing content asset to a
compatible exposed Niagara `User.*` UObject parameter at spawn time. Use it for
Material, Texture, Curve, Static Mesh, or Ribbon-profile inputs; the template is
responsible for exposing and wiring the matching parameter type.

**Bind any projectile Blueprint:**

```text
WTBR.Niagara.AutoBindProjectileBlueprints E:\Path\To\ProjectileVFX.bind.json
```

This writes the same configuration directly to the Class Defaults of any
Blueprint derived from `AWTBRProjectileBase`. Use this for projectile families
without a dedicated Trigger Data Asset VFX struct. Start with
`Source/WTBR/VFXJson/ProjectileVFX.bind.example.json` and replace every example
content path with a real asset.

## No-JSON Preset Editor

Open **Tools → Generate VFX Preset...**, select a template/output path, choose a
color, and enter Energy, Speed, and Spark Count. The editor creates a strict
internal spec under `Saved/VFXPresets`, imports it, and opens the generated asset
in Niagara's Preview Scene. The template must expose `User.Color` (color),
`User.Energy` (float), `User.Speed` (float), and `User.SparkCount` (int).

**Audit static Niagara complexity:**

```text
WTBR.Niagara.AuditSystem /Game/VFX/Sniper/NS_Telorn_Impact
```

The audit reports enabled emitter count, renderer count, CPU/GPU simulation, and
pre-allocation. It does not alter the asset; it warns when static complexity is
above the WTBR gameplay-effect guardrails.
(Unquoted paths with spaces are re-joined automatically.)

Re-running the same JSON updates the existing output asset **in place** (no duplicate
copies, no GUID churn) — edit values in the JSON and re-import to iterate.

---

## JSON Schema (v1)

```json
{
	"schemaVersion": 1,
	"template": "/Game/VFX/Templates/NS_Template_Burst",
	"outputPath": "/Game/VFX/Generated/NS_Test_Burst_FromJson",
	"strict": true,
	"addMissingParams": false,
	"params": {
		"User.Color":      { "type": "color", "value": [0.45, 0.30, 1.00, 1.0] },
		"User.Lifetime":   { "type": "float", "value": 0.6 },
		"User.Size":       { "type": "float", "value": 24.0 },
		"User.SpawnCount": { "type": "int",   "value": 32 }
	}
}
```

| Field | Required | Notes |
|-------|----------|-------|
| `schemaVersion` | Recommended | Production schema version. Currently `1`; omitted v0.1 specs remain compatible. |
| `template` | ✅ | Content path of an existing `UNiagaraSystem`. **Whitelist: must be under `/Game/VFX/Templates/`** (E004) — prevents duplicating gameplay assets by accident. Missing on disk / wrong class → E101/E102. |
| `outputPath` | ✅ | Must be under `/Game/VFX/` (E006) and **never under `/Game/VFX/Templates/`** (E007 — templates are read-only inputs). Existing package on disk → update in place; exists but won't load as a NiagaraSystem → refuse (E201). |
| `strict` | No (default `false`) | When `true`, every JSON parameter must already exist on the template with the exact Niagara type. Any mismatch aborts before the output asset is touched. Recommended for production specs. |
| `addMissingParams` | — (default `false`, **always**) | `true` = params not exposed on the template are **added** to the exposed store. A param added this way is *not wired to any module*, so it stores a value but drives nothing visually — explicit per-file opt-in only. Non-bool value → W005, treated as `false`. |
| `params` | ✅ | Object. Keys accept both `User.Color` and `Color` (normalized via `MakeUserVariable`). Each entry must be `{ "type": ..., "value": ... }`. Empty object → W004. |

Unknown top-level fields → W001 warning, import continues.

Supported `type` values: `float`, `int`, `bool`, `color` (`[r,g,b]` or `[r,g,b,a]`, 0–1 floats), `vector2`, `vector3`, and `vector4` (aliases `vec2`, `vec3`/`vector`, and `vec4`).

Mismatched type vs. the template's exposed param → W102 warning + that param skipped
(the store's type wins; the value is never force-cast).

### Pre-Flight Validation & Error/Warning Codes (Phase A)

The whole JSON spec is schema-validated **before any asset is loaded, duplicated,
modified, or saved**. Any E-code aborts the import with `E000` and a guarantee that
no asset was touched; W-codes log and continue. All problems in a file are reported
in one pass (not fail-fast).

| Code | Meaning |
|------|---------|
| E000 | Summary line: pre-flight failed, nothing was touched |
| E001 / E002 | File unreadable / invalid JSON |
| E003 / E004 | `template` missing-or-empty / outside `/Game/VFX/Templates/` whitelist |
| E005 / E006 / E007 | `outputPath` missing-or-empty / outside `/Game/VFX/` / under the Templates root |
| E008 | `params` missing or not an object |
| E009 / E010 / E011 / E012 / E013 | Param entry: not an object / missing `type` / unsupported type / missing-or-wrong-shape `value` / empty key |
| W001 | Unknown top-level field (ignored) |
| W004 / W005 | `params` empty / `addMissingParams` not a bool (default `false` used) |
| E101 / E102 | Template asset not found / not a NiagaraSystem |
| W201 | Asset Registry has no record of the output package even though it exists on disk (stale in this process) — a targeted forced rescan runs automatically before load |
| E201 | Output package exists on disk but could not be loaded as a NiagaraSystem — import aborts, does **not** fall back to DuplicateAsset (would overwrite it) |
| E202 | DuplicateAsset blocked — destination package appeared on disk between the existence check and the write (race-condition guard) |
| E104 / E105 | DuplicateAsset failed / SaveLoadedAsset failed |
| W101 / W102 / W103 | Runtime param skips: not exposed on template / type mismatch / SetParameterValue failed |
| W104–W107 | Runtime backstops for shapes already rejected pre-flight (should not appear in practice) |

### Existence Detection: Disk Is the Source of Truth (not the Asset Registry)

`ImportFromFile` decides create-vs-update by calling `FPackageName::DoesPackageExist`
(resolves directly against the mounted content root files) — **not**
`UEditorAssetLibrary::DoesAssetExist` alone, which only consults the Asset
Registry cache. A fresh headless process has to scan `Content/` on startup to
discover packages a *different* process wrote; if that scan hasn't caught up
yet, the registry-only check returns `false` for a package that is really on
disk, and the importer would wrongly duplicate over an existing asset instead
of updating it in place (silently discarding anything about the existing
asset the JSON doesn't control). When the disk check finds the package, the
importer forces a targeted synchronous rescan of just that directory
(`IAssetRegistry::ScanPathsSynchronous(..., bForceRescan=true)`) before
loading it, and it **never** falls back to `DuplicateAsset` once a package is
confirmed to exist — a load failure there is a hard abort (`E201`), not a
silent overwrite.

**Known limitation: this only sees what's on disk.** The fix removes the
Asset-Registry-staleness bug, but it does not — and cannot — make a headless
import aware of another process's in-memory state. Supported vs. unsupported
workflows:

**Supported:**
- Import via **Tools menu** or **console command**, run inside the *same*
  Unreal Editor session that has the asset open.
- **Headless import while Unreal Editor is closed** (no other process has the
  project open at all).

**Unsupported / not guaranteed:**
- **Headless import while a separate Unreal Editor process has the same
  project open** — especially if that Editor has the target asset loaded in
  memory. Even if the asset was saved before the headless run started, the
  open Editor GUI may still be holding an older in-memory copy of the object
  and can **save over** the values the headless process just wrote the next
  time *it* saves (autosave, closing the asset editor, project save, etc.).
  There is no cross-process locking here — **do not run concurrent GUI +
  headless edits against the same production asset.**

**Unsaved edits:** a headless import process cannot see unsaved edits made in
a different Editor process — it only ever reads what's actually been written
to the `.uasset` file. Production workflows should therefore either import
inside the same Editor session as any manual edits, or close the Editor
before running a separate headless import against that asset.

---

## Production Template Validation (one command)

Once `NS_Template_Burst` exists (section above), run:

```powershell
powershell -File "Plugins\NiagaraJsonGenerator\Tools\RunProductionValidation.ps1"
```

It launches one headless editor pass (`Tools/RunProductionValidation.py`) that:

1. **Round 1** — deletes any stale output, imports
   `Examples/NS_Test_Burst_FromJson.json` → creates
   `/Game/VFX/Generated/NS_Test_Burst_FromJson`, dumps all User Parameter values.
2. **Round 2** — imports `Examples/NS_Test_Burst_FromJson_Round2.json` (same
   `outputPath`, changed Color/Size/SpawnCount) → must take the **update-in-place**
   path and the dump must show the new values.
3. **Negative** — imports `Examples/Invalid/NS_Invalid_MissingParam.json`
   (`User.DoesNotExist`, `addMissingParams: false`) → must **warn + skip** (W101), and
   the final dump must NOT contain the bogus param.
4. **Phase A schema fixtures** — each must be rejected pre-flight (correct E-code
   logged, output asset must NOT exist afterwards), plus one warn-but-import case:

   | Fixture | Expects |
   |---------|---------|
   | `Invalid/NS_Invalid_TemplateOutsideWhitelist.json` | E004, nothing created |
   | `Invalid/NS_Invalid_OutputUnderTemplates.json` | E007, nothing created |
   | `Invalid/NS_Invalid_MissingTypeField.json` | E010, nothing created |
   | `Invalid/NS_Invalid_BadValueShape.json` | E011 + E012 (collected in one pass), nothing created |
   | `NS_Warn_UnknownTopLevel.json` | W001 warning **and** a successful import (asset cleaned up by the script) |

The wrapper prints the exact UnrealEditor-Cmd command line, then greps the run's
log and prints a PASS/FAIL line per assertion (exit 0 = all green, exit 2 =
template missing, exit 1 = readable failure: editor crashed / python didn't run /
console commands didn't execute / an assertion failed). The same commands work
interactively too: `WTBR.Niagara.ImportJson <json>` and
`WTBR.Niagara.DumpUserParams <contentPath>` in the editor console.

**Safe to run while Unreal Editor is open.** The commandlet writes to its own
dedicated log via `-abslog` (`Saved/Logs/NiagaraJsonValidation.log` — this is the
evidence file, deleted fresh each run; editor stdout is captured next to it as
`NiagaraJsonValidation.log.stdout.txt`). Do NOT look for results in `WTBR.log`:
when the editor is open it locks that file and a second instance would silently
write `WTBR_2.log` instead — that mismatch is exactly the failure mode the
dedicated log removes.

## Fresh-Process Regression Test (update-in-place across processes)

`RunProductionValidation.ps1`'s Round 1/Round 2 both run inside **one**
UnrealEditor-Cmd process, so they can never catch a registry-timing bug: by
the time Round 2 runs, that process's own Round-1 `DuplicateAsset` call has
already populated its in-memory Asset Registry. The bug only shows up when a
**different, brand-new process** has to discover a package that a prior
process wrote — exactly the situation a real production pipeline hits when
two separate tool invocations (or a headless import after a manual editor
session) touch the same output asset.

```powershell
powershell -File "Plugins\NiagaraJsonGenerator\Tools\RunFreshProcessRegressionTest.ps1"
```

It launches **two independent** UnrealEditor-Cmd processes, Step B started
only after Step A has fully exited:

1. **Step A** (`FreshProcess_StepA.py`) imports
   `Examples/NS_FreshProcess_A_CreateWithSentinel.json` — creates
   `/Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test` and plants a
   `User.SentinelMarker` bool (via `addMissingParams: true`, test-only) that
   Step B's JSON never mentions.
2. **Step B** (`FreshProcess_StepB.py`), in a fresh process that has never
   seen this asset, imports `Examples/NS_FreshProcess_B_UpdateOnly.json`
   (same `outputPath`, different Color/Lifetime/Size/SpawnCount,
   `addMissingParams: false`) and dumps the result.

Assertions (on Step B's log): it logged **`Updated`**, never
`Created`/`Duplicated`; no `E201`/`E202` abort; the four JSON-controlled
params reflect Step B's values; and `SentinelMarker` — which Step B's JSON
never touches — is still `true`. Whether `W201` actually fired (i.e. whether
this particular run's registry happened to be stale) is logged as `INFO`
only, not a pass/fail gate — it's a timing race that depends on how fast that
process's initial Content scan finishes, not something the fix controls; the
outcome assertions above are what actually prove correctness either way.

Covers the **Supported: headless import while Editor is closed** workflow
(both steps are plain headless processes with no interactive Editor
involved). It does **not** and cannot cover the concurrent-GUI-Editor
scenario — see "Known limitation" two sections up: that requires a live
Editor session holding the asset, which is exactly the unsupported workflow,
and isn't something a headless pair can safely automate or claim to prove
safe.

This test asset is scratch: `Remove-TestAsset` deletes
`Content/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test*` both before and
after the run (`try`/`finally`, so a failed assertion or a crashed step still
leaves it cleaned up) — the runner is safe to re-run any number of times and
never leaves anything for `git status` to pick up.

### In-Editor Visual Checklist (human eyes — the part automation can't do)

Run after the automated checks pass:

- [ ] Open `/Game/VFX/Generated/NS_Test_Burst_FromJson` in the Niagara Editor —
      opens clean, same emitter structure as the template.
- [ ] **User Parameters panel** shows the Round 2 values: Color `(1.0, 0.35, 0.1)`,
      Lifetime `0.6`, Size `60`, SpawnCount `128` — not the template defaults.
- [ ] **Drag the asset into a level.** With the template's `Infinite` loop it pulses
      every second: expect a dense (128) burst of large (60) orange particles.
- [ ] Edit `Examples/NS_Test_Burst_FromJson_Round2.json` — e.g. Color to green
      `[0.2, 1.0, 0.3, 1.0]`, SpawnCount to `16` — and re-import via
      **Tools → Import Niagara JSON...** (or the console command).
- [ ] **The placed effect in the level must visibly change** (sparse green instead of
      dense orange). If the viewport doesn't refresh, reselect the actor or toggle
      its Activate — the asset on disk is already updated (verify with
      `WTBR.Niagara.DumpUserParams /Game/VFX/Generated/NS_Test_Burst_FromJson`).

**Go/no-go:** all automated checks PASS + every box above ticked ⇒ the
`JSON → duplicate → set params → save` production workflow is GO; next step is the
template library + full importer (see Next Steps). Any box fails ⇒ file the failure
in this README before extending anything.

---

## Verification Checklist (spike acceptance criteria — round 1, historical)

After running the import:

1. **Asset created** — Content Browser shows `/Game/VFX/Generated/NS_Test_Burst_FromJson`.
2. **Opens in Niagara Editor** — double-click the new asset; it must open without errors
   and show the same emitters as the template.
3. **Values actually changed** — in the opened asset's **User Parameters** panel check:
   `Color = (0.45, 0.30, 1.0, 1.0)`, `Lifetime = 0.6`, `Size = 24`, `SpawnCount = 32`
   (i.e. NOT the defaults you typed when creating the template).
4. **Persistence** — close Unreal Editor completely, reopen, open the asset again:
   values from step 3 must still be there.
5. **No graph creation** — emitter/module stack of the generated asset is byte-identical
   in structure to the template (only exposed parameter defaults differ).
6. **Log check** — Output Log filtered to `LogNiagaraJsonSpike` shows one
   `Set User Parameter: User.<Name>` line per param, `skipped: 0, warnings: 0` in the
   summary.

Negative checks worth 2 minutes:

- Rename a param in the JSON to something not on the template with
  `addMissingParams: false` → expect a "not exposed on template" warning, other params
  still applied, editor does not crash.
- Change `"type": "float"` to `"type": "color"` on `User.Lifetime` → expect a type
  mismatch warning + skip.

---

## Implementation Notes / API Used (verified against UE 5.1.1 source)

| Step | API |
|------|-----|
| Duplicate | `UEditorAssetLibrary::DuplicateAsset` (EditorScriptingUtilities) |
| Exposed param store | `UNiagaraSystem::GetExposedParameters()` → `FNiagaraUserRedirectionParameterStore&` (NiagaraSystem.h:317) |
| Name normalization | `FNiagaraUserRedirectionParameterStore::MakeUserVariable` — makes `Color` ≡ `User.Color` |
| Enumerate existing | `FNiagaraParameterStore::GetParameters(TArray<FNiagaraVariable>&)` |
| Set value | `FNiagaraParameterStore::SetParameterValue<T>(value, var, bAdd)` — sizeof(T) must equal the Niagara type size: `float`/`int32`/`FNiagaraBool` = 4, `FVector3f` = 12, `FLinearColor` = 16 |
| Save | `UEditorAssetLibrary::SaveLoadedAsset(asset, false)` |

## Known Limitations Found While Building

- **`bool` must go through `FNiagaraBool`, not C++ `bool`** — `SetParameterValue`
  `check()`s `sizeof(T)` against the Niagara type size (4 bytes); a raw `bool` (1 byte)
  would assert.
- **Vec3 is `FVector3f`, not `FVector`** — UE 5.1 LWC: the store's Vec3 slot is 12
  bytes; passing a double-based `FVector` (24 bytes) would assert. The importer uses
  `FVector3f` explicitly.
- **`addMissingParams: true` adds a *dangling* parameter** — it appears in the User
  Parameters panel and persists, but drives nothing until a human links it inside a
  module. It is useful for controlled experiments but production specs should use
  `strict: true`, which treats a missing template parameter as an error.
- **No visual verification possible from code** — the importer confirms values are
  stored, not that the effect looks right. Human eyes (or a future screenshot loop)
  still gate quality.
- **Python alone could not do this cleanly in 5.1.1** — setting user-parameter
  *defaults on the asset* isn't exposed to Python scripting (same class of gap as the
  UMG `widget_tree` finding from the T1 spike); the C++ editor module is what makes it
  work. Asset duplication alone *is* available to Python.

## Recommended Next Content Steps

1. Expand the curated template library: `NS_Base_SlashTrail`, `NS_Base_HitBurst`, `NS_Base_Beam`, and `NS_Base_GroundImpact` with an agreed User Parameter naming convention.
2. Put `"schemaVersion": 1` and `"strict": true` in every new gameplay VFX spec.
3. Group weapon families into batch manifests and run validation before import in CI or a headless editor process.
