param(
    [string]$ArchiveDirectory = "E:\World Trigger\WTBR_Builds\NoPak_Dev_Codex_01",
    [string]$ProjectPath = "E:\World Trigger\WTBR\WTBR.uproject",
    [string]$EngineRoot = "E:\UE_5.1"
)

$runUat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"

& $runUat BuildCookRun `
    -project="$ProjectPath" `
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
    -archivedirectory="$ArchiveDirectory"
