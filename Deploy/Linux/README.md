# RPGDemo dedicated server host

The host uses a non-login `rpgdemo` system account. Releases are immutable directories under
`/opt/rpgdemo/releases/<build-id>` and `/opt/rpgdemo/current` is the active symlink.

1. Copy this directory to the Ubuntu 22.04 host and run `sudo ./install-host.sh` once.
2. Add the operator's SSH public key to the existing authorized Ubuntu account. Do not commit keys.
3. Open only UDP 7777 in the cloud security group and, when enabled, UFW.
4. Deploy from Windows with `Tools/Deploy-LinuxServer.ps1 -SshHost <ssh-alias> -Archive <zip>`.

Logs are available through `journalctl -u rpgdemo.service` and the packaged game's `Saved/Logs`.
