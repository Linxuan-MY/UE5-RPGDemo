[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$SshHost,
    [Parameter(Mandatory)] [string]$Archive,
    [ValidateSet('/opt/rpgdemo')] [string]$InstallRoot = '/opt/rpgdemo',
    [string]$IdentityFile,
    [ValidateRange(1, 65535)] [int]$SshPort = 22
)

$ErrorActionPreference = 'Stop'
# Releases live under /opt/rpgdemo/releases and current is switched atomically.
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$hashPath = "$archivePath.sha256"
if (-not (Test-Path -LiteralPath $hashPath)) { throw "Missing checksum file: $hashPath" }
$buildId = [IO.Path]::GetFileNameWithoutExtension($archivePath)
if ($buildId -notmatch '^[A-Za-z0-9._-]+$' -or $buildId -in @('.', '..')) { throw 'Unsafe release build id.' }
if ($SshHost.StartsWith('-') -or $SshHost -match '\s') { throw 'Invalid SSH host.' }
$checksum = (Get-Content -LiteralPath $hashPath -Raw).Trim()
if ($checksum -notmatch '^([A-Fa-f0-9]{64})  (.+)$') { throw 'Invalid checksum file format.' }
$expectedHash = $Matches[1]
if ($Matches[2] -ne [IO.Path]::GetFileName($archivePath)) { throw 'Checksum file names a different archive.' }
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $expectedHash) { throw 'Local archive checksum mismatch.' }
$sshOptions = @('-T', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=15', '-p', "$SshPort")
$scpOptions = @('-o', 'BatchMode=yes', '-o', 'ConnectTimeout=15', '-P', "$SshPort")
if ($IdentityFile) {
    $identityPath = (Resolve-Path -LiteralPath $IdentityFile).Path
    $sshOptions += @('-i', $identityPath, '-o', 'IdentitiesOnly=yes')
    $scpOptions += @('-i', $identityPath, '-o', 'IdentitiesOnly=yes')
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $entryNames = @($zip.Entries | ForEach-Object FullName)
    foreach ($requiredEntry in @('RPGDemoServer.sh', 'RPGDemo/Binaries/Linux/RPGDemoServer')) {
        if ($entryNames -notcontains $requiredEntry) {
            throw "Archive is missing required entry: $requiredEntry"
        }
    }
}
finally {
    $zip.Dispose()
}
$remoteUpload = "/tmp/$([IO.Path]::GetFileName($archivePath))"
$remoteHash = "$remoteUpload.sha256"

& ssh @sshOptions -- $SshHost 'sudo -n true && id rpgdemo >/dev/null && systemctl cat rpgdemo.service >/dev/null'
if ($LASTEXITCODE -ne 0) { throw 'SSH authentication or host provisioning preflight failed; nothing uploaded.' }
& scp @scpOptions -- $archivePath "${SshHost}:$remoteUpload"
if ($LASTEXITCODE -ne 0) { throw 'Archive upload failed.' }
& scp @scpOptions -- $hashPath "${SshHost}:$remoteHash"
if ($LASTEXITCODE -ne 0) { throw 'Checksum upload failed.' }

$script = @'
set -euo pipefail
upload="$1"
checksum="$2"
install_root="$3"
build_id="$4"
case "$install_root" in /opt/rpgdemo) ;; *) echo "unsafe install root" >&2; exit 2;; esac
case "$build_id" in *[!A-Za-z0-9._-]*) echo "unsafe build id" >&2; exit 2;; esac
release="$install_root/releases/$build_id"
previous=""
if [ -L "$install_root/current" ]; then
    previous="$(readlink -f "$install_root/current")"
    case "$previous" in "$install_root"/releases/*) ;; *) echo "current points outside releases" >&2; exit 2;; esac
elif [ -e "$install_root/current" ]; then
    echo "current must be a symlink" >&2; exit 2
fi
cd /tmp
sha256sum -c "$checksum"
sudo install -d -o rpgdemo -g rpgdemo "$install_root/releases"
if sudo test -e "$release" || sudo test -e "$release.tmp"; then
    echo "release already exists; use a new build id" >&2; exit 2
fi
sudo install -d -o rpgdemo -g rpgdemo "$release.tmp"
sudo -u rpgdemo unzip -q "$upload" -d "$release.tmp"
sudo -u rpgdemo chmod +x \
    "$release.tmp/RPGDemoServer.sh" \
    "$release.tmp/RPGDemo/Binaries/Linux/RPGDemoServer"
sudo mv -- "$release.tmp" "$release"
next_current="$install_root/.current-$build_id"
sudo ln -s -- "$release" "$next_current"
sudo mv -Tf -- "$next_current" "$install_root/current"
deployment_ok=false
if sudo systemctl restart rpgdemo.service; then
    for attempt in $(seq 1 120); do
        if sudo systemctl is-active --quiet rpgdemo.service && \
           sudo ss -H -lun | awk '{print $5}' | grep -Eq '(^|:)7777$'; then
            deployment_ok=true
            break
        fi
        sleep 1
    done
fi
if [ "$deployment_ok" != true ]; then
    echo "deployment failed readiness probe; starting rollback" >&2
    sudo systemctl stop rpgdemo.service
    if [ -n "$previous" ]; then
        sudo ln -s -- "$previous" "$next_current"
        sudo mv -Tf -- "$next_current" "$install_root/current"
        sudo systemctl start rpgdemo.service
    else
        sudo rm -f -- "$install_root/current"
    fi
    exit 1
fi
rm -f -- "$upload" "$checksum"
'@

# Encode the LF script so Windows native stdin newline conversion cannot corrupt Bash.
$lfScript = $script.Replace(([string][char]13 + [char]10), [string][char]10) + [char]10
$encodedScript = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($lfScript))
$remoteCommand = "printf %s $encodedScript | base64 -d | bash -s -- $remoteUpload $remoteHash $InstallRoot $buildId"
& ssh @sshOptions -- $SshHost $remoteCommand
if ($LASTEXITCODE -ne 0) { throw 'Remote deployment or rollback failed.' }
Write-Host "Deployed $buildId to $SshHost"
