[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $projectRoot 'RPGDemo.uproject'
$pythonScriptPath = Join-Path $PSScriptRoot 'verify_cooldown_widget_isolation.py'
$editorPath = 'E:\UE engine\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logDirectory = Join-Path $projectRoot 'Saved\Logs'
$logPath = Join-Path $logDirectory 'CooldownWidgetIsolationTest.log'

if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe was not found at: $editorPath"
}

if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
    throw "Project file was not found at: $projectPath"
}

if (-not (Test-Path -LiteralPath $pythonScriptPath -PathType Leaf)) {
    throw "Verification script was not found at: $pythonScriptPath"
}

if (-not (Test-Path -LiteralPath $logDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $logDirectory | Out-Null
}

if (Test-Path -LiteralPath $logPath -PathType Leaf) {
    Remove-Item -LiteralPath $logPath
}

& $editorPath `
    $projectPath `
    "-ExecutePythonScript=$pythonScriptPath" `
    "-abslog=$logPath" `
    -unattended `
    -nop4 `
    -nosplash `
    -NoSound `
    -nullrhi

$editorExitCode = $LASTEXITCODE
if ($editorExitCode -ne 0) {
    Write-Error "Unreal Editor exited with code $editorExitCode. See $logPath"
    exit $editorExitCode
}

if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    Write-Error "Unreal Editor did not create the expected log: $logPath"
    exit 1
}

$logLines = Get-Content -LiteralPath $logPath
$logText = $logLines -join [Environment]::NewLine

if ($logText -notmatch 'COOLDOWN_WIDGET_CHECK_COMPILATION_BEGIN' -or
    $logText -notmatch 'COOLDOWN_WIDGET_CHECK_COMPILATION_END') {
    Write-Error "The Widget Blueprint compilation did not complete. See $logPath"
    exit 1
}

$compilationBlock = [Regex]::Match(
    $logText,
    '(?s)COOLDOWN_WIDGET_CHECK_COMPILATION_BEGIN(?<body>.*?)COOLDOWN_WIDGET_CHECK_COMPILATION_END'
)
if (-not $compilationBlock.Success) {
    Write-Error "Could not isolate the Widget Blueprint compilation log. See $logPath"
    exit 1
}

$blueprintCompilerErrorPattern =
    '(LogBlueprint|LogK2Compiler|CompilerResultsLog).*Error:'
if ($compilationBlock.Groups['body'].Value -match $blueprintCompilerErrorPattern) {
    Write-Error "TPWBP_AbilityIconSlot produced a Blueprint compiler error. See $logPath"
    exit 1
}

if ($logText -notmatch 'COOLDOWN_WIDGET_CHECK_DISASSEMBLY_BEGIN' -or
    $logText -notmatch 'COOLDOWN_WIDGET_CHECK_DISASSEMBLY_END') {
    Write-Error "The Blueprint disassembly did not complete. See $logPath"
    exit 1
}

if ($logText -match 'COOLDOWN_WIDGET_CHECK_ASSET_LOAD_FAILED') {
    Write-Error "The target Widget Blueprint could not be loaded. See $logPath"
    exit 1
}

$snapshotName = 'ActiveCooldownTotalTime'
if ($logText -notmatch [Regex]::Escape($snapshotName)) {
    Write-Error (
        "Cooldown isolation regression: the compiled widget has no " +
        "'$snapshotName' slot-owned snapshot."
    )
    exit 1
}

$conversionMarker = 'CallFunc_MapRangeClamped_InRangeA_ImplicitCast'
$conversionLineIndexes = for ($index = 0; $index -lt $logLines.Count; $index++) {
    if ($logLines[$index] -match [Regex]::Escape($conversionMarker)) {
        $index
    }
}

if (-not $conversionLineIndexes) {
    Write-Error "Could not locate the MapRangeClamped InRangeA conversion in compiled bytecode."
    exit 1
}

$usesSlotSnapshot = $false
$stillUsesEventParameter = $false
foreach ($index in $conversionLineIndexes) {
    $windowStart = [Math]::Max(0, $index - 24)
    $windowEnd = [Math]::Min($logLines.Count - 1, $index + 24)
    $window = ($logLines[$windowStart..$windowEnd] -join [Environment]::NewLine)

    if ($window -match [Regex]::Escape($snapshotName)) {
        $usesSlotSnapshot = $true
        if ($window -match 'K2Node_CustomEvent_TotalCooldownTime') {
            $stillUsesEventParameter = $true
        }
    }
}

if (-not $usesSlotSnapshot) {
    Write-Error (
        "Cooldown isolation regression: MapRangeClamped.InRangeA does not read " +
        "'$snapshotName'."
    )
    exit 1
}

if ($stillUsesEventParameter) {
    Write-Error (
        "Cooldown isolation regression: MapRangeClamped.InRangeA still reads " +
        "the shared event parameter K2Node_CustomEvent_TotalCooldownTime."
    )
    exit 1
}

Write-Host (
    "[PASS] TPWBP_AbilityIconSlot uses its slot-owned cooldown total for " +
    "MapRangeClamped.InRangeA."
)
exit 0
