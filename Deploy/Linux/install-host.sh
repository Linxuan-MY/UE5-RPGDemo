#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "Run as root: sudo $0" >&2
  exit 1
fi

if ! id rpgdemo >/dev/null 2>&1; then
  useradd --system --home-dir /opt/rpgdemo --shell /usr/sbin/nologin rpgdemo
fi
install -d -o rpgdemo -g rpgdemo /opt/rpgdemo/releases
install -m 0644 "$(dirname "$0")/rpgdemo.service" /etc/systemd/system/rpgdemo.service
systemctl daemon-reload
systemctl enable rpgdemo.service

echo "Host runtime installed. Add only UDP 7777 to the cloud security group and firewall."
echo "If UFW is active: sudo ufw allow 7777/udp"
