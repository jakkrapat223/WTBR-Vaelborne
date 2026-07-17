# Static Niagara budget gate for local use and CI.
param([string]$EnginePath = "E:\UE_5.1")

$ErrorActionPreference = "Stop"
$ToolDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = (Resolve-Path (Join-Path $ToolDir "..\..\..")).Path
$Project = Get-ChildItem $ProjectRoot -Filter *.uproject | Select-Object -First 1
$ManifestPath = Join-Path $ProjectRoot "Source\WTBR\VFXJson\VFXPerformanceGate.json"
$PythonScript = Join-Path $ToolDir "RunVFXPerformanceGate.py"
$EditorCmd = Join-Path $EnginePath "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$LogFile = Join-Path $ProjectRoot "Saved\Logs\VFXPerformanceGate.log"

if (!$Project -or !(Test-Path $EditorCmd)) { throw "Project or UnrealEditor-Cmd.exe not found." }
$Spec = Get-Content $ManifestPath -Raw | ConvertFrom-Json
Remove-Item $LogFile -Force -ErrorAction SilentlyContinue

& $EditorCmd $Project.FullName -run=pythonscript "-script=$PythonScript" "-abslog=$LogFile" -stdout -unattended -nosplash -nullrhi
if ($LASTEXITCODE -ne 0 -or !(Test-Path $LogFile)) { throw "Unreal audit run failed." }

$Log = Get-Content $LogFile -Raw
$Failed = 0
foreach ($Asset in $Spec.assets) {
    $Escaped = [regex]::Escape($Asset)
    $Pattern = "VFX audit \| $Escaped \| Emitters=(?<Emitters>\d+) \(CPU=(?<Cpu>\d+) GPU=(?<Gpu>\d+)\) \| Renderers=(?<Renderers>\d+) \| PreAllocation=(?<PreAllocation>\d+)"
    $Match = [regex]::Match($Log, $Pattern)
    if (!$Match.Success) { Write-Host "FAIL  no audit result: $Asset"; $Failed++; continue }
    $OverBudget = ([int]$Match.Groups['Emitters'].Value -gt $Spec.budget.maxEmitters) -or
        ([int]$Match.Groups['Renderers'].Value -gt $Spec.budget.maxRenderers) -or
        ([int]$Match.Groups['Gpu'].Value -gt $Spec.budget.maxGpuEmitters) -or
        ([int]$Match.Groups['PreAllocation'].Value -gt $Spec.budget.maxPreAllocation)
    if ($OverBudget) { Write-Host "FAIL  budget exceeded: $Asset"; $Failed++ }
    else { Write-Host "PASS  $Asset" }
}
if ($Failed -gt 0) { throw "$Failed VFX performance budget violation(s)." }
Write-Host "VFX performance gate passed."
