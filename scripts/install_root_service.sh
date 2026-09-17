#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "[*] Installing WattCurb as native Administrator (Root) System Service..."

# 1. Stop and disable legacy user-level daemon
sudo -u jedclub XDG_RUNTIME_DIR="/run/user/1000" systemctl --user stop wattcurb.service 2>/dev/null || true
sudo -u jedclub XDG_RUNTIME_DIR="/run/user/1000" systemctl --user disable wattcurb.service 2>/dev/null || true
rm -f /home/jedclub/.config/systemd/user/wattcurb.service
rm -f /dev/shm/wattcurb_state.shm
pkill -9 -f "/home/jedclub/.local/bin/wattcurb --daemon" 2>/dev/null || true

# 2. Install binaries to /usr/local/bin and ~/.local/bin
install -m 755 "${REPO_DIR}/build/wattcurb" /usr/local/bin/wattcurb
install -m 755 "${REPO_DIR}/build/wattcurb" /home/jedclub/.local/bin/wattcurb
install -m 755 "${REPO_DIR}/build/wattcurb-tray" /home/jedclub/.local/bin/wattcurb-tray
install -m 755 "${REPO_DIR}/build/wattcurb-dashboard" /home/jedclub/.local/bin/wattcurb-dashboard

# 3. Install systemd system unit
cat << 'EOF' > /etc/systemd/system/wattcurb.service
[Unit]
Description=WattCurb Ultra-Low-Overhead Power Profiling & Mitigation Daemon (Root System Service)
Documentation=https://github.com/jedclub/WattCurb
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/bin/wattcurb --daemon --period 3.0
Restart=on-failure
RestartSec=3
KillMode=mixed
TimeoutStopSec=2

[Install]
WantedBy=multi-user.target
EOF

# 4. Reload and restart system root service
systemctl daemon-reload
systemctl enable wattcurb.service
systemctl restart wattcurb.service

# 5. Restart user-level tray client
sudo -u jedclub XDG_RUNTIME_DIR="/run/user/1000" systemctl --user restart wattcurb-tray.service 2>/dev/null || true

echo "[+] Success! WattCurb background daemon is now running natively as Root."
echo "[+] Root privileges are permanently active from boot with ZERO password/polkit prompts."
