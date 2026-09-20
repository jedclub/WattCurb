#!/usr/bin/env bash
# WattCurb Universal Production Installer
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/output"
[ ! -d "${BIN_DIR}" ] && BIN_DIR="${SCRIPT_DIR}/bin"

if [ ! -f "${BIN_DIR}/wattcurb" ]; then
    echo "[!] Error: Precompiled binaries not found in ${BIN_DIR}."
    echo "    Please run 'bash scripts/build_pgo.sh' first or download the release archive."
    exit 1
fi

echo "==================================================================="
echo "  ⚡ Installing WattCurb System & Desktop Suite                    "
echo "==================================================================="

# 1. Check root or prompt sudo
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        echo "[!] Error: Root privileges are required to install systemd root service."
        exit 1
    fi
fi

# 2. Install Binaries
echo "[1/4] Installing binaries to /usr/local/bin..."
${SUDO} install -m 755 "${BIN_DIR}/wattcurb" /usr/local/bin/wattcurb
${SUDO} install -m 755 "${BIN_DIR}/wattcurb-tray" /usr/local/bin/wattcurb-tray
if [ -f "${BIN_DIR}/wattcurb-dashboard" ]; then
    ${SUDO} install -m 755 "${BIN_DIR}/wattcurb-dashboard" /usr/local/bin/wattcurb-dashboard
fi

# Also link into ~/.local/bin if directory exists
if [ -d "${HOME}/.local/bin" ]; then
    install -m 755 "${BIN_DIR}/wattcurb" "${HOME}/.local/bin/wattcurb"
    install -m 755 "${BIN_DIR}/wattcurb-tray" "${HOME}/.local/bin/wattcurb-tray"
    if [ -f "${BIN_DIR}/wattcurb-dashboard" ]; then
        install -m 755 "${BIN_DIR}/wattcurb-dashboard" "${HOME}/.local/bin/wattcurb-dashboard"
    fi
fi

# 3. Install Systemd Root Service
echo "[2/4] Installing root background service..."
${SUDO} tee /etc/systemd/system/wattcurb.service > /dev/null << 'EOF'
[Unit]
Description=WattCurb Ultra-Low-Overhead Power Profiling & Mitigation Daemon
Documentation=https://github.com/jedclub/WattCurb
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/bin/wattcurb --daemon --period 10.0
Restart=on-failure
RestartSec=3
KillMode=mixed
TimeoutStopSec=2

[Install]
WantedBy=multi-user.target
EOF

${SUDO} systemctl daemon-reload
${SUDO} systemctl enable --now wattcurb.service

# 4. Install Desktop Autostart & User Tray Service
echo "[3/4] Configuring desktop autostart..."
mkdir -p "${HOME}/.config/autostart"
cat << 'EOF' > "${HOME}/.config/autostart/wattcurb-tray.desktop"
[Desktop Entry]
Name=WattCurb Tray
Comment=WattCurb Ultra-Low-Overhead Power Indicator
Exec=/usr/local/bin/wattcurb-tray
Icon=battery-good
Terminal=false
Type=Application
Categories=Utility;System;
StartupNotify=false
X-GNOME-Autostart-enabled=true
EOF

# 5. Launch Tray Indicator
echo "[4/4] Starting desktop tray indicator..."
pkill -f "wattcurb-tray" 2>/dev/null || true
nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &

echo "==================================================================="
echo "  ✅ WattCurb Installation Complete!                              "
echo "==================================================================="
echo "  • Daemon Status : sudo systemctl status wattcurb.service"
echo "  • Live Status   : wattcurb --status"
echo "  • Executive View: wattcurb --briefing"
echo "  • Tray Indicator: Running in system tray"
echo "==================================================================="
