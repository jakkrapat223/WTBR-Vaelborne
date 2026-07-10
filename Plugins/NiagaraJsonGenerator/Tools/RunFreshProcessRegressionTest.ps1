# Fresh-process regression test for the registry-timing fix (see README:
# "Existence Detection: Disk Is the Source of Truth").
#
# The bug this guards against: a headless process that did NOT create an
# asset itself has to discover it via an Asset Registry scan on startup. If
# that scan hasn't caught up when ImportJson runs, a registry-only existence
# check returns false for a package that is really on disk, and the importer
# would wrongly duplicate over it instead of updating in place.
#
# This can only be reproduced with TWO SEPARATE OS processes — Round 1/Round 2
# in RunProductionValidation.ps1 both run inside one process, which never hits
# this bug (the process's own DuplicateAsset call already populated its
# in-memory registry). Step A and Step B below are launched as two independent
# UnrealEditor-Cmd invocations, Step B started only after Step A has fully
# exited.
#
# The test asset this creates is scratch — cleaned up before AND after every
# run (try/finally, so a failed assertion or a crashed process still leaves a
# clean Content/ tree) — never intended to be committed.
#
# Usage: powershell -File RunFreshProcessRegressionTest.ps1 [-EnginePath E:\UE_5.1]
param(
    [string]$EnginePath = "E:\UE_5.1"
)

$ErrorActionPreference = "Stop"

$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path (Join-Path $ScriptDir "..\..\..")).Path
$UProject    = Get-ChildItem $ProjectRoot -Filter *.uproject | Select-Object -First 1
$EditorCmd   = Join-Path $EnginePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

$StepAScript = Join-Path $ScriptDir "FreshProcess_StepA.py"
$StepBScript = Join-Path $ScriptDir "FreshProcess_StepB.py"
$LogA        = Join-Path $ProjectRoot "Saved\Logs\NiagaraFreshProcess_StepA.log"
$LogB        = Join-Path $ProjectRoot "Saved\Logs\NiagaraFreshProcess_StepB.log"

# Test asset lives directly under Content/VFX/Generated/ as a single package
# file. An uncooked NiagaraSystem asset has no separate sidecar files, but
# the cleanup globs on the base name (scoped to this one directory) rather
# than a single hard-coded filename, so it also catches any stray leftover
# (e.g. a ".uasset.tmp" from an interrupted save) without touching anything
# else in Generated/.
$TestAssetDir  = Join-Path $ProjectRoot "Content\VFX\Generated"
$TestAssetBase = "NS_FreshProcess_UpdateInPlace_Test"

function Remove-TestAsset {
    param([string]$Label)
    if (-not (Test-Path $TestAssetDir)) { return }
    $Stale = Get-ChildItem -Path $TestAssetDir -Filter "$TestAssetBase*" -File -ErrorAction SilentlyContinue
    foreach ($F in $Stale) {
        Write-Host "Cleanup ($Label): removing $($F.FullName)"
        Remove-Item -LiteralPath $F.FullName -Force -Confirm:$false
    }
}

if (-not (Test-Path $EditorCmd)) {
    Write-Host "FAIL: UnrealEditor-Cmd.exe not found at: $EditorCmd (pass -EnginePath)"; exit 1
}
if ($null -eq $UProject) {
    Write-Host "FAIL: no .uproject found in: $ProjectRoot"; exit 1
}

function Invoke-Step {
    param([string]$Label, [string]$PyScript, [string]$LogFile)
    $EditorArgs = @(
        "`"$($UProject.FullName)`"",
        "-run=pythonscript",
        "-script=`"$PyScript`"",
        "-abslog=`"$LogFile`"",
        "-stdout", "-unattended", "-nosplash", "-nullrhi"
    )
    Write-Host ""
    Write-Host "--- $Label ---"
    Write-Host "  `"$EditorCmd`" $($EditorArgs -join ' ')"
    $StdOutFile = "$LogFile.stdout.txt"
    $StdErrFile = "$LogFile.stderr.txt"
    $Proc = Start-Process -FilePath $EditorCmd -ArgumentList $EditorArgs -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $StdOutFile -RedirectStandardError $StdErrFile
    Write-Host "$Label exit code: $($Proc.ExitCode)"
    if (-not (Test-Path $LogFile)) {
        Write-Host "FAIL: $Label produced no log file: $LogFile"; exit 1
    }
    return (Get-Content $LogFile -Raw)
}

$ExitCode = 1
try {
    # Fresh disk state — Step A must always start clean, and this makes the
    # runner repeatable without anyone manually deleting the asset first.
    Remove-TestAsset -Label "pre-run"

    # Step A and Step B are two INDEPENDENT processes. Step A fully exits
    # (Wait) before Step B is ever launched — nothing about the asset
    # survives in memory between them, which is exactly what makes this a
    # fresh-process test.
    $LogAText = Invoke-Step -Label "STEP A (create + plant sentinel)" -PyScript $StepAScript -LogFile $LogA
    $LogBText = Invoke-Step -Label "STEP B (fresh process, update only)" -PyScript $StepBScript -LogFile $LogB

    # Whether the Asset Registry happens to be stale in Step B's fresh process
    # (triggering W201) is a timing race, not something this script controls —
    # it depends on how fast that process's initial Content scan completes
    # before the console command runs. It is informational, NOT a pass/fail
    # gate: the behavior that actually matters (disk-truth branching via
    # FPackageName::DoesPackageExist) is correct either way, so the hard
    # assertions below check the OUTCOME (update-in-place, correct values,
    # sentinel survival), not whether the race was won or lost this run.
    if ($LogBText -match "W201: ") {
        Write-Host "INFO  Step B: hit the stale-registry path (W201) - race reproduced this run"
    } else {
        Write-Host "INFO  Step B: registry was already in sync (no W201) - race not hit this run, harmless"
    }

    $Checks = [ordered]@{
        "Step A: asset created from template (4 existing params set + 1 sentinel added)" =
            ($LogAText -match [regex]::Escape("Created /Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test | params set: 4, added: 1, skipped: 0"));
        "Step A: sentinel present after creation" =
            ($LogAText -match "User\.SentinelMarker.*= true");
        "Step B: took the update-in-place path (not Created/Duplicated)" =
            ($LogBText -match [regex]::Escape("updating User Parameters in place")) -and
            (-not ($LogBText -match "Duplicated template")) -and
            (-not ($LogBText -match "LogNiagaraJsonSpike: Created "));
        "Step B: logged Updated with 4 params set, 0 skipped" =
            ($LogBText -match [regex]::Escape("Updated /Game/VFX/Generated/NS_FreshProcess_UpdateInPlace_Test | params set: 4, added: 0, skipped: 0"));
        "Step B: no E201/E202 abort" =
            (-not ($LogBText -match "E201: ")) -and (-not ($LogBText -match "E202: "));
        "Step B: Color/Lifetime/Size/SpawnCount now reflect B's values" =
            ($LogBText -match "User\.Color.*R=0\.90+,G=0\.10+,B=0\.40+") -and
            ($LogBText -match "User\.Lifetime.*= 0\.9") -and
            ($LogBText -match "User\.Size.*= 77") -and
            ($LogBText -match "User\.SpawnCount.*= 5");
        "Step B: sentinel SURVIVED (not touched by B's JSON)" =
            ($LogBText -match "User\.SentinelMarker.*= true");
    }

    $Failed = 0
    foreach ($Name in $Checks.Keys) {
        if ($Checks[$Name]) { Write-Host "PASS  $Name" }
        else { Write-Host "FAIL  $Name"; $Failed++ }
    }

    Write-Host ""
    if ($Failed -eq 0) {
        Write-Host "ALL FRESH-PROCESS REGRESSION CHECKS PASSED."
        Write-Host "  Step A log: $LogA"
        Write-Host "  Step B log: $LogB"
        $ExitCode = 0
    } else {
        Write-Host "$Failed check(s) FAILED - inspect $LogA and $LogB (filter: LogNiagaraJsonSpike)."
        $ExitCode = 1
    }
}
finally {
    # Always runs — pass, fail, or a process/exit call partway through —
    # so this test never leaves a scratch asset sitting in git status.
    Remove-TestAsset -Label "post-run"
}

exit $ExitCode
