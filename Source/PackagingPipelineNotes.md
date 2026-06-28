# Vaelborne Packaging Pipeline Notes

## Current status

- Manual `RunUAT` NoPak packaging is working for `Win64 Development`.
- Packaged NoPak smoke is passing.
- Editor `Package Project` / launcher-driven packaging is still blocked for Pak builds.

## Findings

- Project packaging defaults currently disable Pak and IoStore in `Config/DefaultGame.ini`.
- `Saved\Config\WindowsEditor\Game.ini` stores staging and packaging session state, but it does not contain an override forcing `UsePakFile=True` or `bUseIoStore=True`.
- `Build\Windows\FileOpenOrder\CookerOpenOrder.log` and `EditorOpenOrder.log` both parse cleanly as `"path" index` entries.
- The editor log captured the UI packaging path serializing UAT with `-pak -compressed`, and in one run also `-iostore`, even though the project defaults were already set for NoPak.

## Root-cause hypothesis

The reliable evidence points to the editor packaging path using stale or internally forced packaging flags when it serializes the `BuildCookRun` command, instead of honoring the current NoPak project defaults end-to-end.

This looks like an editor or launcher-profile state issue, not a malformed project config override:

- `DefaultGame.ini` says `UsePakFile=False` and `bUseIoStore=False`
- cooked metadata preserved `UsePakFile=False` and `bUseIoStore=False`
- no saved editor config file was found re-enabling those flags
- the editor still launched UAT with `-pak -compressed`

Because the failing path only appears once Pak packaging is forced back on, the current Pak failure remains blocked in the editor-driven pipeline.

## Recommended build path

Use the manual NoPak UAT command or the helper script in `Source\BuildScripts\Package-NoPak-Dev.ps1`.

Exact command:

```powershell
& "E:\UE_5.1\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  -project="E:\World Trigger\WTBR\WTBR.uproject" `
  -noP4 `
  -utf8output `
  -platform=Win64 `
  -clientconfig=Development `
  -target=WTBR `
  -build `
  -cook `
  -stage `
  -package `
  -archive `
  -clean `
  -prereqs `
  -archivedirectory="E:\World Trigger\WTBR_Builds\NoPak_Dev_Codex_01"
```

Required absence:

- no `-pak`
- no `-iostore`
- no `-compressed`

## Recommended packaged smoke

Host listen server:

```powershell
& "E:\World Trigger\WTBR_Builds\NoPak_Dev_Codex_01\Windows\WTBR.exe" "/Game/ThirdPerson/Maps/ThirdPersonMap?listen" -log
```

Join client:

```powershell
& "E:\World Trigger\WTBR_Builds\NoPak_Dev_Codex_01\Windows\WTBR.exe" "127.0.0.1" -log
```

Short smoke checklist:

- clients connect to listen host
- projectile damage applies
- melee damage applies
- validation logs stay quiet by default

## Pak status

Normal Pak packaging is still blocked.

Known failing path:

- editor `Package Project` or launcher-driven packaging emits `-pak -compressed`
- Pak step then fails during `CreatePakUsingStagingManifest` / `OrderFile`
- reported error: `System.ArgumentOutOfRangeException: startIndex cannot be larger than length of string`
