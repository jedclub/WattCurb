#!/usr/bin/env bash
# WattCurb Uninstaller
set -euo pipefail

echo "[*] Uninstalling WattCurb..."

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    fi
fi

# 1. Stop and disable services
${SUDO} systemctl stop wattcurb.service 2>/dev/null || true
${SUDO} systemctl disable wattcurb.service 2>/dev/null || true
pkill -f "wattcurb-tray" 2>/dev/null || true
pkill -f "wattcurb-dashboard" 2>/dev/null || true
pkill -f "wattcurb --daemon" 2>/dev/null || true

# 2. Remove systemd units, autostart, and desktop entries
${SUDO} rm -f /etc/systemd/system/wattcurb.service
${SUDO} systemctl daemon-reload 2>/dev/null || true
${SUDO} rm -f /etc/xdg/autostart/wattcurb-tray.desktop
${SUDO} rm -f /usr/share/applications/wattcurb-dashboard.desktop
rm -f "${HOME}/.config/autostart/wattcurb-tray.desktop"
rm -f "${HOME}/.config/systemd/user/wattcurb-tray.service"
if [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER}" != "root" ]; then
    USER_HOME=$(getent passwd "${SUDO_USER}" | cut -d: -f6)
    rm -f "${USER_HOME}/.config/autostart/wattcurb-tray.desktop" 2>/dev/null || true
fi

# 3. Remove binaries
${SUDO} rm -f /usr/local/bin/wattcurb
${SUDO} rm -f /usr/local/bin/wattcurb-tray
${SUDO} rm -f /usr/local/bin/wattcurb-dashboard
rm -f "${HOME}/.local/bin/wattcurb"
rm -f "${HOME}/.local/bin/wattcurb-tray"
rm -f "${HOME}/.local/bin/wattcurb-dashboard"

# 4. Clean shared memory
rm -f /dev/shm/wattcurb_state.shm
rm -f /dev/shm/wattcurb_history.shm

echo "[+] WattCurb successfully uninstalled."
