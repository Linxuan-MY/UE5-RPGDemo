# RPGDemo dedicated server host

The host uses a non-login `rpgdemo` system account. Releases are immutable directories under
`/opt/rpgdemo/releases/<build-id>` and `/opt/rpgdemo/current` is the active symlink.

1. Copy this directory to the Ubuntu 22.04 host and run `sudo ./install-host.sh` once.
2. Add the operator's SSH public key to the existing authorized Ubuntu account. Do not commit keys.
3. Open only UDP 7777 in the cloud security group and, when enabled, UFW.
4. Deploy from Windows with `Tools/Deploy-LinuxServer.ps1 -SshHost <ssh-alias> -Archive <zip>`.

Logs are available through `journalctl -u rpgdemo.service` and the packaged game's `Saved/Logs`.

Build both packages with PowerShell 7 using `Tools/Build-LinuxServer.ps1` and
`Tools/Build-WindowsClient.ps1`. Use the same `-BuildId` and `-SourceSnapshotPath`
from `Tools/New-ReleaseSnapshot.ps1` to enforce identical source inputs. Each zip
includes `release-source.json`, with SHA256 hashes of uncommitted project changes;
its adjacent `.manifest.json` and `.sha256` describe the published package.

`Deploy-LinuxServer.ps1` accepts `-IdentityFile` and `-SshPort` for SSH login.
The SSH operator needs noninteractive sudo access for provisioning/deployment.
Deployment validates the local checksum before upload, checks host provisioning,
switches `current` with an atomic rename, and restores the previous release if
startup fails. A first-deployment failure stops the service and removes only the
active symlink. Existing release directories are never overwritten.

The startup probe checks service state and UDP 7777 for up to 120 seconds. Confirm
public connectivity with two matching clients and test lobby-to-arena travel.
