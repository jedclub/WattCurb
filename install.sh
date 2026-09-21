#!/usr/bin/env bash
# WattCurb Universal Production Installer
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="${SCRIPT_DIR}/output"
[ ! -d "${BIN_DIR}" ] && BIN_DIR="${SCRIPT_DIR}/bin"
[ ! -f "${BIN_DIR}/wattcurb" ] && [ -f "${SCRIPT_DIR}/build/wattcurb" ] && BIN_DIR="${SCRIPT_DIR}/build"

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
# REF-REQ-106.4: install the repository unit verbatim. An inline copy here
# silently reverts the REF-REQ-093 service hardening on every install.
UNIT_SRC="${SCRIPT_DIR}/scripts/wattcurb.service"
if [ ! -f "${UNIT_SRC}" ]; then
    echo "[!] Error: ${UNIT_SRC} not found; refusing to install an unhardened unit."
    exit 1
fi
${SUDO} install -m 644 "${UNIT_SRC}" /etc/systemd/system/wattcurb.service

${SUDO} systemctl daemon-reload
${SUDO} systemctl enable --now wattcurb.service

# 4. Install Desktop Autostart & Application Entries
echo "[3/4] Configuring desktop autostart and application entries..."
# 4.1. System-wide XDG autostart (ensures tray starts automatically on any user login)
${SUDO} mkdir -p /etc/xdg/autostart
${SUDO} tee /etc/xdg/autostart/wattcurb-tray.desktop > /dev/null << 'EOF'
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
X-KDE-autostart-after=panel
X-systemd-skip=true
EOF

# 4.2. User-specific autostart backup
TARGET_USER="${SUDO_USER:-$USER}"
if [ -n "${TARGET_USER}" ] && [ "${TARGET_USER}" != "root" ]; then
    USER_HOME=$(getent passwd "${TARGET_USER}" | cut -d: -f6)
    if [ -d "${USER_HOME}" ]; then
        mkdir -p "${USER_HOME}/.config/autostart"
        cp /etc/xdg/autostart/wattcurb-tray.desktop "${USER_HOME}/.config/autostart/wattcurb-tray.desktop"
        chown "${TARGET_USER}:${TARGET_USER}" "${USER_HOME}/.config/autostart/wattcurb-tray.desktop" 2>/dev/null || true
    fi
fi

# 4.3. System-wide Desktop Entry for Dashboard
if [ -f "${BIN_DIR}/wattcurb-dashboard" ]; then
    ${SUDO} mkdir -p /usr/share/applications
    ${SUDO} tee /usr/share/applications/wattcurb-dashboard.desktop > /dev/null << 'EOF'
[Desktop Entry]
Name=WattCurb Matrix Dashboard
Comment=WattCurb Ultra-Low-Overhead Power & Metric Matrix
Exec=/usr/local/bin/wattcurb-dashboard
Icon=utilities-system-monitor
Terminal=false
Type=Application
Categories=Utility;System;Monitor;
EOF
fi

# 5. Launch Desktop Tray Indicator in Active Graphical Session
echo "[4/4] Starting desktop tray indicator in user graphical session..."
if [ -n "${TARGET_USER}" ] && [ "${TARGET_USER}" != "root" ]; then
    TARGET_UID=$(id -u "${TARGET_USER}" 2>/dev/null || echo "1000")
    TARGET_RUNTIME="/run/user/${TARGET_UID}"

    # Terminate any existing tray client for this user
    sudo -u "${TARGET_USER}" pkill -f "wattcurb-tray" 2>/dev/null || true

    # Launch tray under the desktop user's graphical session environment
    if [ -d "${TARGET_RUNTIME}" ]; then
        sudo -u "${TARGET_USER}" \
            XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
            DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
            nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
    else
        sudo -u "${TARGET_USER}" nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
    fi
else
    pkill -f "wattcurb-tray" 2>/dev/null || true
    nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
fi

echo "==================================================================="
echo "  ✅ WattCurb Installation Complete!                              "
echo "==================================================================="
echo "  • Daemon Status : sudo systemctl status wattcurb.service (Active & Autostart)"
echo "  • Live Status   : wattcurb --status"
echo "  • Executive View: wattcurb --briefing"
echo "  • Tray Indicator: Running in system tray (Auto-launches on desktop login)"
echo "==================================================================="
