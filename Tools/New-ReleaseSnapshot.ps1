[CmdletBinding()]
param(
    [string]$EngineRoot = 'E:\UE Source\UnrealEngine-5.8',
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$commit = (& git -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve the project commit.' }
$engineCommit = (& git -C $EngineRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Unable to resolve the source engine commit.' }
$engineVersion = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
$changedPaths = @(& git -c core.quotepath=false -C $projectRoot diff --name-only HEAD --)
if ($LASTEXITCODE -ne 0) { throw 'Unable to enumerate changed project files.' }
$changedPaths += @(& git -c core.quotepath=false -C $projectRoot ls-files --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'Unable to enumerate untracked project files.' }
$files = @(foreach ($relativePath in ($changedPaths | Sort-Object -Unique)) {
    $path = Join-Path $projectRoot $relativePath
    $exists = Test-Path -LiteralPath $path -PathType Leaf
    [pscustomobject]@{
        Path = $relativePath
        Exists = $exists
        Sha256 = if ($exists) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    }
})
$fingerprintText = @($commit, $engineCommit, ($engineVersion | ConvertTo-Json -Compress), ($files | ConvertTo-Json -Depth 5 -Compress)) -join [char]10
$sha = [Security.Cryptography.SHA256]::Create()
try { $fingerprint = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($fingerprintText))).Replace('-', '').ToLowerInvariant() }
finally { $sha.Dispose() }
$snapshot = [pscustomobject]@{
    CreatedUtc = [DateTime]::UtcNow.ToString('o')
    ProjectCommit = $commit
    EngineCommit = $engineCommit
    EngineVersion = $engineVersion
    HasUncommittedChanges = $files.Count -gt 0
    SourceFingerprint = $fingerprint
    ChangedFiles = $files
}
if ($OutputPath) {
    $destination = [IO.Path]::GetFullPath($OutputPath)
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    $snapshot | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $destination -Encoding utf8
}
$snapshot
