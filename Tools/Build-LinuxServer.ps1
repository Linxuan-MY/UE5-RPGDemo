[CmdletBinding()]
param(
    [string]$EngineRoot = 'E:\UE Source\UnrealEngine-5.8',
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    [ValidateRange(1, 32)]
    [int]$MaxParallelActions = 8,
    [string]$ArchiveRoot = (Join-Path $PSScriptRoot '..\Artifacts\LinuxServer')
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'RPGDemo.uproject'
$runUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $runUat)) { throw "RunUAT was not found at $runUat" }

$commit = (& git -C $projectRoot rev-parse --short=12 HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve the project commit.' }
$buildId = "$(Get-Date -Format 'yyyyMMdd-HHmmss')-$commit"
$archiveDirectory = Join-Path $ArchiveRoot $buildId
New-Item -ItemType Directory -Force -Path $archiveDirectory | Out-Null

& $runUat BuildCookRun `
    -project=$projectFile `
    -noP4 -utf8output -build -cook -stage -pak -archive `
    -server -noclient -serverplatform=Linux `
    -serverconfig=$Configuration `
    -target=RPGDemoServer `
    "-UbtArgs=-MaxParallelActions=$MaxParallelActions" `
    -map=/Game/Maps/MultiplayerLobbyMap+/Game/Maps/MainMenuMap+/Game/Maps/SurvivalGameModeMap `
    -archivedirectory=$archiveDirectory
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed with exit code $LASTEXITCODE" }

$archiveFile = Join-Path $ArchiveRoot "$buildId.zip"
Compress-Archive -Path (Join-Path $archiveDirectory '*') -DestinationPath $archiveFile -Force
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $archiveFile
"$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($archiveFile))" |
    Set-Content -LiteralPath "$archiveFile.sha256" -Encoding ascii

[pscustomobject]@{ BuildId = $buildId; Archive = $archiveFile; Sha256 = $hash.Hash.ToLowerInvariant() }
