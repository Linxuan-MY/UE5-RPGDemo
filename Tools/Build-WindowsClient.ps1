[CmdletBinding()]
param(
    [string]$EngineRoot = 'E:\UE Source\UnrealEngine-5.8',
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    [ValidateRange(1, 32)]
    [int]$MaxParallelActions = 8,
    [string]$ArchiveRoot = (Join-Path $PSScriptRoot '..\Artifacts\WindowsClient'),
    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$BuildId,
    [string]$SourceSnapshotPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'RPGDemo.uproject'
$runUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $runUat)) { throw "RunUAT was not found at $runUat" }
$commit = (& git -C $projectRoot rev-parse --short=12 HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve the project commit.' }
if (-not $BuildId) { $BuildId = "$(Get-Date -Format 'yyyyMMdd-HHmmss')-$commit" }
$snapshotTool = Join-Path $PSScriptRoot 'New-ReleaseSnapshot.ps1'
$snapshot = & $snapshotTool -EngineRoot $EngineRoot
if ($SourceSnapshotPath) {
    $expected = Get-Content -LiteralPath $SourceSnapshotPath -Raw | ConvertFrom-Json
    if ($snapshot.SourceFingerprint -ne $expected.SourceFingerprint) { throw 'Project inputs differ from the requested release snapshot.' }
}
$archiveDirectory = Join-Path $ArchiveRoot $BuildId
if (Test-Path -LiteralPath $archiveDirectory) { throw "Release directory already exists: $archiveDirectory" }
New-Item -ItemType Directory -Force -Path $archiveDirectory | Out-Null
$uatArguments = @(
    'BuildCookRun', "-project=$projectFile", '-noP4', '-utf8output', '-build', '-cook', '-stage', '-pak', '-archive',
    '-platform=Win64', "-clientconfig=$Configuration", '-target=RPGDemo',
    "-UbtArgs=-MaxParallelActions=$MaxParallelActions",
    '-map=/Game/Maps/MainMenuMap+/Game/Maps/MultiplayerLobbyMap+/Game/Maps/SurvivalGameModeMap',
    "-archivedirectory=$archiveDirectory"
)
& $runUat @uatArguments
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed with exit code $LASTEXITCODE" }
$after = & $snapshotTool -EngineRoot $EngineRoot
if ($after.SourceFingerprint -ne $snapshot.SourceFingerprint) { throw 'Project inputs changed during the client build; do not publish this output.' }
$packageRoot = $archiveDirectory
if (-not (Test-Path -LiteralPath (Join-Path $packageRoot 'RPGDemo.exe'))) { $packageRoot = Join-Path $archiveDirectory 'Windows' }
if (-not (Test-Path -LiteralPath (Join-Path $packageRoot 'RPGDemo.exe'))) { throw 'Missing packaged client entry: RPGDemo.exe' }
$snapshot | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $packageRoot 'release-source.json') -Encoding utf8
$archiveFile = Join-Path $ArchiveRoot "$BuildId.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($packageRoot, $archiveFile, [IO.Compression.CompressionLevel]::Optimal, $false)
$hash = Get-FileHash -LiteralPath $archiveFile -Algorithm SHA256
"$($hash.Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($archiveFile))" | Set-Content -LiteralPath "$archiveFile.sha256" -Encoding ascii
$result = [pscustomobject]@{ BuildId = $BuildId; Platform = 'Win64'; Configuration = $Configuration; Archive = [IO.Path]::GetFullPath($archiveFile); Sha256 = $hash.Hash.ToLowerInvariant(); SourceFingerprint = $snapshot.SourceFingerprint }
$result | ConvertTo-Json | Set-Content -LiteralPath "$archiveFile.manifest.json" -Encoding utf8
$result
