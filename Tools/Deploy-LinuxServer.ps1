[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string]$SshHost,
    [Parameter(Mandatory)] [string]$Archive,
    [string]$InstallRoot = '/opt/rpgdemo'
)

$ErrorActionPreference = 'Stop'
# Releases live under /opt/rpgdemo/releases and current is switched atomically.
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$hashPath = "$archivePath.sha256"
if (-not (Test-Path -LiteralPath $hashPath)) { throw "Missing checksum file: $hashPath" }
$buildId = [IO.Path]::GetFileNameWithoutExtension($archivePath)
$remoteUpload = "/tmp/$([IO.Path]::GetFileName($archivePath))"
$remoteHash = "$remoteUpload.sha256"

& scp -- $archivePath "${SshHost}:$remoteUpload"
if ($LASTEXITCODE -ne 0) { throw 'Archive upload failed.' }
& scp -- $hashPath "${SshHost}:$remoteHash"
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
if [ -L "$install_root/current" ]; then previous="$(readlink -f "$install_root/current")"; fi
cd /tmp
sha256sum -c "$checksum"
sudo install -d -o rpgdemo -g rpgdemo "$install_root/releases"
sudo rm -rf -- "$release.tmp"
sudo install -d -o rpgdemo -g rpgdemo "$release.tmp"
sudo -u rpgdemo unzip -q "$upload" -d "$release.tmp"
sudo mv -- "$release.tmp" "$release"
sudo ln -sfn -- "$release" "$install_root/current"
deployment_ok=false
if sudo systemctl restart rpgdemo.service; then
    for attempt in $(seq 1 30); do
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
    if [ -n "$previous" ]; then sudo ln -sfn -- "$previous" "$install_root/current"; fi
    sudo systemctl restart rpgdemo.service
    exit 1
fi
rm -f -- "$upload" "$checksum"
'@

$script | & ssh -- $SshHost 'bash -s --' $remoteUpload $remoteHash $InstallRoot $buildId
if ($LASTEXITCODE -ne 0) { throw 'Remote deployment or rollback failed.' }
Write-Host "Deployed $buildId to $SshHost"
