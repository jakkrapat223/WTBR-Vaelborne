# WBP_HUD_Generated re-import safety — decision record

**Status:** decided 2026-07-18 (Claude + Codex, owner-approved). Supersedes the
"paused, unverified" framing in the 2026-07-17 session notes. Read this before
touching `WBP_HUD_Generated` or its `UMGJsonGenerator` import pipeline again.

## Problem

`WBP_HUD_Generated` already carries live production behavior (HP/VAEL bars,
Main/Sub Trigger name+cost+slot indicator, all refreshed from
`UWTBRHUDViewModelComponent`'s snapshot). The Composite Merge Bar UI (Phase 1+2
mockup, HTML/CSS/JSON already built and verified at the source level) has never
been imported into this asset because `FUMGJsonImporter::ClearWidgetTree`
performs a full in-place tree rebuild, and it was originally unclear whether
that rebuild would destroy the live bindings.

## Verified facts (checked against the real code 2026-07-18, not assumed)

- The live bindings are **native C++ `BindWidgetOptional`** properties on
  `UWTBRGeneratedHUDWidget` (`Source/WTBR/UI/WTBRGeneratedHUDWidget.h:38` etc.,
  e.g. `PB_HP`, `Txt_HPValue`, `PB_Vael`, `Txt_MainTriggerName`), refreshed via
  `NativeConstruct()`/`RefreshFromViewModel()`
  (`Source/WTBR/UI/WTBRGeneratedHUDWidget.cpp:10`) — **not** UMG's runtime
  Property Binding feature.
- `FUMGJsonImporter::ClearWidgetTree`
  (`Plugins/UMGJsonGenerator/Source/UMGJsonGeneratorEditor/Private/UMGJsonImporter.cpp:547`)
  only empties `UWidgetBlueprint::Bindings` (UMG Property Bindings) and
  `::Animations` before moving old widgets to the transient package and
  rebuilding. **`BindWidgetOptional` properties are not stored in either
  array** — they are re-resolved by the reflection system matching
  Name+Type against the rebuilt `WidgetTree` at construct time.
- Consequence: native bindings have a **high chance of surviving** an in-place
  rebuild, provided all of these hold after the rebuild:
  1. `WBP_HUD_Generated`'s parent class is still `UWTBRGeneratedHUDWidget`
     (importer preserves the existing parent class unless the JSON explicitly
     overrides it).
  2. Every currently-bound widget name is unchanged (`PB_HP`, `Txt_HPValue`,
     `PB_Vael`, `Txt_VaelValue`, `Txt_MainTriggerName`, etc.).
  3. Each widget's type still matches its C++ property type exactly (e.g.
     `PB_HP` stays a `ProgressBar`, not swapped to an `Image`).
  4. Each widget is still marked `bIsVariable` (required for `WidgetTree` to
     expose it by name to the reflection lookup `BindWidgetOptional` uses).
  5. The regenerated WBP still compiles cleanly.
- **Not yet verified**: whether any Blueprint Event Graph nodes reference the
  old widget objects directly (as opposed to the native C++ path above). If
  `WBP_HUD_Generated` has hand-added Graph logic beyond the native parent
  class, those references could still break — this is why a real probe is
  required, not just a code-reading argument.

## Decision: sandboxed Reimport Probe, not dry-run alone

A dry-run/preview (diffing JSON against the current tree without touching
anything) cannot answer the real question, because it never actually clears
the widget object, rebuilds it, recompiles the generated class, or constructs
a runtime instance — all of which are exactly the steps that could break a
binding. The agreed tool is a **Reimport Probe**:

1. Duplicate `WBP_HUD_Generated` → `WBP_HUD_ReimportProbe` (a real, separate
   asset — never touches the production asset).
2. Clone the target JSON (Composite Merge Bar mockup JSON) with
   `widgetName` repointed at the probe.
3. Run the real import pipeline against the probe end-to-end (clear tree,
   rebuild, compile).
4. Diff before/after: parent class, Graph node/function list, property
   bindings, animations, widget names+types, compile status.
5. Load the probe in PIE and manually confirm HP/VAEL and Q/E switching still
   update live.
6. Delete the probe once a human has confirmed the result either way.

Only after this probe passes does the *real* `WBP_HUD_Generated` get
re-imported — and even then, take a package-level backup first (see caveat
below).

## Agreed order (supersedes the earlier "P1 first" framing)

1. Build the Reimport Probe workflow + before/after report.
2. Add a package backup step for the real re-import (duplicate the asset
   before touching it).
3. Run the Composite Merge Bar JSON through the probe; PIE-verify HP/VAEL +
   Q/E survive.
4. **If bindings/Graph survive**: proceed with the real in-place rebuild using
   the now-proven-safe pipeline. No need to build P4's non-destructive
   diff/merge — it would be solving a problem that doesn't exist here.
5. **If bindings/Graph do NOT survive**: only then invest in P4's
   non-destructive diff/merge (edit only the widgets that actually changed,
   leave everything else untouched) — that is the real fix for that failure
   mode, not a bigger backup/rollback story.
6. Everything else in the original P1–P4 proposal (schema versioning, strict
   validation, richer widget types/layout, batch import, CI commandlet,
   WBP→JSON export, etc.) is good hygiene but does not block this task —
   defer until after the HUD import is unblocked one way or the other.

## Caveat: "backup" is not a full rollback

Duplicating an asset before editing it is a *convenience* copy, not a true
rollback — other assets that reference `WBP_HUD_Generated` by its exact
package path will still point at the (now-modified) original, not the backup
duplicate. A true rollback means restoring the original `.uasset` bytes from
source control (git/LFS) or a package-level backup, not just keeping a
same-content duplicate around. The Reimport Probe sidesteps this entirely by
never modifying the production asset until the probe has already proven the
pipeline safe.

## References
- [[wtbr-composite-merge-hud-mockup-state]] (chat memory) — Phase 1+2 mockup
  status, the original "paused" decision this document supersedes.
- `Plugins/UMGJsonGenerator/Examples/GameUI/WBP_HUD_BindingPlan.md` — the
  existing audit of what is/isn't wired on `WBP_HUD_Generated` and why.
