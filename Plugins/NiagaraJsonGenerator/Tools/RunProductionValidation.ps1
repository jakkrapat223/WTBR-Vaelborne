# Production template validation runner + log assertions.
# Usage (from anywhere):  powershell -File RunProductionValidation.ps1 [-EnginePath E:\UE_5.1]
#
# Safe to run while Unreal Editor is open: the commandlet writes to its own
# dedicated log file via -abslog, so it never collides with (or reads) the
# open editor's WTBR.log.
param(
    [string]$EnginePath = "E:\UE_5.1"
)

$ErrorActionPreference = "Stop"

$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path (Join-Path $ScriptDir "..\..\..")).Path
$UProject    = Get-ChildItem $ProjectRoot -Filter *.uproject | Select-Object -First 1
$PyScript    = Join-Path $ScriptDir "RunProductionValidation.py"
$EditorCmd   = Join-Path $EnginePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

# Dedicated log file — NOT WTBR.log. A running editor locks WTBR.log, which
# silently pushes a second instance to WTBR_2.log; -abslog removes the guess.
$LogFile     = Join-Path $ProjectRoot "Saved\Logs\NiagaraJsonValidation.log"

if (-not (Test-Path $EditorCmd)) {
    Write-Host "FAIL: UnrealEditor-Cmd.exe not found at: $EditorCmd (pass -EnginePath)"; exit 1
}
if ($null -eq $UProject) {
    Write-Host "FAIL: no .uproject found in: $ProjectRoot"; exit 1
}

# Fresh log every run — never read stale output from a previous attempt.
if (Test-Path $LogFile) { Remove-Item $LogFile -Force -Confirm:$false }

$EditorArgs = @(
    "`"$($UProject.FullName)`"",
    "-run=pythonscript",
    "-script=`"$PyScript`"",
    "-abslog=`"$LogFile`"",
    "-stdout", "-unattended", "-nosplash", "-nullrhi"
)

Write-Host "Command line:"
Write-Host "  `"$EditorCmd`" $($EditorArgs -join ' ')"
Write-Host ""
Write-Host "Running headless validation (this loads the editor once, ~1-2 min)..."
$StdOutFile = "$LogFile.stdout.txt"
$StdErrFile = "$LogFile.stderr.txt"
$Proc = Start-Process -FilePath $EditorCmd -ArgumentList $EditorArgs -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $StdOutFile -RedirectStandardError $StdErrFile

# ── Tiered diagnostics: each failure mode gets a readable verdict ────────────
if (-not (Test-Path $LogFile)) {
    Write-Host "FAIL: expected log file was not created: $LogFile"
    Write-Host "(-abslog not honored, or the editor crashed before logging? Exit code was $($Proc.ExitCode). Check the command line above.)"; exit 1
}
$Log = Get-Content $LogFile -Raw

if ($Log -notmatch [regex]::Escape("NiagaraJson production validation: begin")) {
    Write-Host "FAIL: Python validation script never ran (no begin marker in $LogFile)."
    Write-Host "Check: PythonScriptPlugin enabled? Script path correct? See log for LogPythonScriptCommandlet errors."; exit 1
}
if ($Log -match "VALIDATION ABORT") {
    Write-Host "ABORT: template /Game/VFX/Templates/NS_Template_Burst not found."
    Write-Host "Create it first (plugin README: One-Time Template Setup), then re-run."
    exit 2
}
# NOTE on exit codes: the commandlet tallies every UE_LOG Error — including the
# E-codes our negative fixtures INTENTIONALLY trigger — and then exits non-zero.
# So a non-zero exit only means "crashed" if the script never reached its end
# marker; otherwise the tally is expected and the assertions below are the truth.
$ScriptCompleted = $Log -match [regex]::Escape("NiagaraJson production validation: end")
if (($Proc.ExitCode -ne 0) -and (-not $ScriptCompleted)) {
    Write-Host "FAIL: UnrealEditor-Cmd exited with code $($Proc.ExitCode) and the validation script did not complete."
    Write-Host "Likely a crash mid-run - inspect $LogFile"; exit 1
}
if ($Log -notmatch "LogNiagaraJsonSpike") {
    Write-Host "FAIL: console commands did not execute - no LogNiagaraJsonSpike lines at all."
    Write-Host "The NiagaraJsonGeneratorEditor module likely failed to load headless; check $LogFile for module/plugin load errors."; exit 1
}

# Everything after the ROUND 2 marker (round-2 dump + negative test).
$Round2Log = ($Log -split [regex]::Escape("--- ROUND 2"))[-1]

$Checks = [ordered]@{
    "Round 1: asset created from template (4 params set, 0 skipped)" =
        ($Log -match [regex]::Escape("Created /Game/VFX/Generated/NS_Test_Burst_FromJson | params set: 4, added: 0, skipped: 0"));
    "Round 2: update-in-place ran (4 params set, 0 skipped)" =
        ($Round2Log -match [regex]::Escape("Updated /Game/VFX/Generated/NS_Test_Burst_FromJson | params set: 4, added: 0, skipped: 0"));
    "Round 2: Color changed to (1.0, 0.35, 0.1)" =
        ($Round2Log -match "User\.Color.*R=1\.0+,G=0\.350*,B=0\.10+");
    "Round 2: Size changed to 60" =
        ($Round2Log -match "User\.Size.*= 60");
    "Round 2: SpawnCount changed to 128" =
        ($Round2Log -match "User\.SpawnCount.*= 128");
    "V1: read-only validation passed" =
        ($Round2Log -match [regex]::Escape("Validation PASSED:"));
    "V1: batch validation passed for both specs" =
        ($Round2Log -match [regex]::Escape("Batch validation complete | succeeded: 2, failed: 0"));
    "Negative: unknown param warned + skipped" =
        ($Round2Log -match "User\.DoesNotExist.*not exposed on template");
    "Negative: unknown param NOT added to asset" =
        (-not ($Round2Log -match "User\.DoesNotExist : "));
    "Phase A: template outside whitelist rejected (E004), nothing created" =
        (($Log -match "E004: ") -and ($Log -match [regex]::Escape("PHASEA NS_Invalid_TemplateOutsideWhitelist.json output_exists = False")));
    "Phase A: outputPath under Templates rejected (E007), nothing created" =
        (($Log -match "E007: ") -and ($Log -match [regex]::Escape("PHASEA NS_Invalid_OutputUnderTemplates.json output_exists = False")));
    "Phase A: param missing type field rejected (E010), nothing created" =
        (($Log -match "E010: ") -and ($Log -match [regex]::Escape("PHASEA NS_Invalid_MissingTypeField.json output_exists = False")));
    "Phase A: unsupported type + bad value shape rejected (E011+E012), nothing created" =
        (($Log -match "E011: ") -and ($Log -match "E012: ") -and ($Log -match [regex]::Escape("PHASEA NS_Invalid_BadValueShape.json output_exists = False")));
    "Phase A: unknown top-level field warns (W001) but still imports" =
        (($Log -match "W001: ") -and ($Log -match [regex]::Escape("PHASEA unknown_field output_exists = True")));
}

$Failed = 0
foreach ($Name in $Checks.Keys) {
    if ($Checks[$Name]) { Write-Host "PASS  $Name" }
    else { Write-Host "FAIL  $Name"; $Failed++ }
}

Write-Host ""
if ($Failed -eq 0) {
    Write-Host "ALL AUTOMATED CHECKS PASSED.  (evidence log: $LogFile)"
    Write-Host "Generated validation assets were cleaned up by the test script."
    exit 0
} else {
    Write-Host "$Failed check(s) FAILED - inspect $LogFile (filter: LogNiagaraJsonSpike)."
    exit 1
}
