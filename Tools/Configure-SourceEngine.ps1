[CmdletBinding()]
param(
    [Parameter()]
    [ValidateNotNullOrEmpty()]
    [string]$EngineRoot = 'E:\UE Source\UnrealEngine-5.8',

    [Parameter()]
    [ValidateRange(1, 64)]
    [int]$MaxParallelActions = 8,

    [Parameter()]
    [switch]$BuildEditor
)

$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'RPGDemo.uproject'
$resolvedEngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$generateProjectFiles = Join-Path $resolvedEngineRoot 'GenerateProjectFiles.bat'
$buildScript = Join-Path $resolvedEngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editorExecutable = Join-Path $resolvedEngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'

foreach ($requiredFile in @($projectFile, $generateProjectFiles, $buildScript, $editorExecutable)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required source-engine file was not found: $requiredFile"
    }
}

$relativeEngineRoot = [System.IO.Path]::GetRelativePath($projectRoot, $resolvedEngineRoot).Replace('\', '/')
$projectText = [System.IO.File]::ReadAllText($projectFile)
$associationPattern = '("EngineAssociation"\s*:\s*")[^"]*(")'

if (-not [System.Text.RegularExpressions.Regex]::IsMatch($projectText, $associationPattern)) {
    throw "EngineAssociation was not found in $projectFile"
}

$projectText = [System.Text.RegularExpressions.Regex]::Replace(
    $projectText,
    $associationPattern,
    { param($match) $match.Groups[1].Value + $relativeEngineRoot + $match.Groups[2].Value },
    1
)
[System.IO.File]::WriteAllText($projectFile, $projectText, [System.Text.UTF8Encoding]::new($false))

& $generateProjectFiles "-project=$projectFile" -game -engine -CurrentPlatform
if ($LASTEXITCODE -ne 0) {
    throw "GenerateProjectFiles failed with exit code $LASTEXITCODE"
}

if ($BuildEditor) {
    & $buildScript RPGDemoEditor Win64 Development "-Project=$projectFile" -WaitMutex -FromMsBuild "-MaxParallelActions=$MaxParallelActions"
    if ($LASTEXITCODE -ne 0) {
        throw "RPGDemoEditor build failed with exit code $LASTEXITCODE"
    }
}

[pscustomobject]@{
    EngineRoot = $resolvedEngineRoot
    EngineAssociation = $relativeEngineRoot
    ProjectFile = $projectFile
    SolutionFile = Join-Path $projectRoot 'RPGDemo.sln'
    EditorBuilt = [bool]$BuildEditor
}
