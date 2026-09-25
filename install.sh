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

# 1. Check root or prompt single escalation (AGENTS.md Sec 14.3)
if [ "$(id -u)" -ne 0 ]; then
    if [ ! -t 0 ] && command -v pkexec >/dev/null 2>&1; then
        exec pkexec bash "$0" "$@"
    elif command -v sudo >/dev/null 2>&1; then
        exec sudo -E bash "$0" "$@"
    elif command -v pkexec >/dev/null 2>&1; then
        exec pkexec bash "$0" "$@"
    else
        echo "[!] Error: Root privileges are required to install systemd root service."
        exit 1
    fi
fi
SUDO=""

# Resolve target desktop user even under root escalation (sudo/pkexec)
TARGET_USER=""
if [ -n "${SUDO_USER:-}" ] && [ "${SUDO_USER}" != "root" ]; then
    TARGET_USER="${SUDO_USER}"
elif [ -n "${PKEXEC_UID:-}" ] && [ "${PKEXEC_UID}" -ne 0 ]; then
    TARGET_USER=$(getent passwd "${PKEXEC_UID}" | cut -d: -f1)
elif [ -n "${USER:-}" ] && [ "${USER}" != "root" ]; then
    TARGET_USER="${USER}"
fi
USER_HOME=""
if [ -n "${TARGET_USER}" ]; then
    USER_HOME=$(getent passwd "${TARGET_USER}" | cut -d: -f6)
fi

# 2. Install Binaries
echo "[1/4] Installing binaries to /usr/local/bin..."
${SUDO} install -m 755 -p "${BIN_DIR}/wattcurb" /usr/local/bin/wattcurb
${SUDO} install -m 755 -p "${BIN_DIR}/wattcurb-tray" /usr/local/bin/wattcurb-tray
if [ -f "${BIN_DIR}/wattcurb-dashboard" ]; then
    ${SUDO} install -m 755 -p "${BIN_DIR}/wattcurb-dashboard" /usr/local/bin/wattcurb-dashboard
fi

# Also symlink into target user's ~/.local/bin if directory exists (preserves /usr/local/bin authorization, REF-REQ-111)
if [ -n "${USER_HOME}" ] && [ -d "${USER_HOME}/.local/bin" ]; then
    rm -f "${USER_HOME}/.local/bin/wattcurb" "${USER_HOME}/.local/bin/wattcurb-tray" "${USER_HOME}/.local/bin/wattcurb-dashboard"
    ln -sf /usr/local/bin/wattcurb "${USER_HOME}/.local/bin/wattcurb"
    ln -sf /usr/local/bin/wattcurb-tray "${USER_HOME}/.local/bin/wattcurb-tray"
    if [ -f "${BIN_DIR}/wattcurb-dashboard" ]; then
        ln -sf /usr/local/bin/wattcurb-dashboard "${USER_HOME}/.local/bin/wattcurb-dashboard"
    fi
    chown -h "${TARGET_USER}:${TARGET_USER}" "${USER_HOME}/.local/bin/wattcurb"* 2>/dev/null || true
fi

# 2.0. Configure persistent thinkpad_acpi fan_control=1 (REF-REQ-114, REF-RES-030)
if [ -d /sys/module/thinkpad_acpi ]; then
    if [ ! -f /etc/modprobe.d/thinkpad_acpi.conf ] || ! grep -q "fan_control=1" /etc/modprobe.d/thinkpad_acpi.conf 2>/dev/null; then
        echo "  • Configuring persistent thinkpad_acpi fan_control=1 in /etc/modprobe.d/thinkpad_acpi.conf..."
        ${SUDO} tee /etc/modprobe.d/thinkpad_acpi.conf > /dev/null << 'EOF'
# WattCurb fan control override (REF-REQ-114, REF-RES-030)
options thinkpad_acpi fan_control=1
EOF
        ${SUDO} chmod 644 /etc/modprobe.d/thinkpad_acpi.conf
    fi
fi

# 2.1. Make ryzenadj reachable for the ROOT daemon (REF-REQ-115.1).
#
# Performance and Balanced raise the SMU power/thermal limits through ryzenadj.
# Without this step the daemon cannot find the tool - it searches only
# /usr/local/bin and /usr/bin - so an EC-imposed cap is never lifted. Measured on
# 2026-09-22: the EC held STAPM at 6 W and Tctl at 70 C, pinning the CPU at
# 550-600 MHz in Performance mode while every knob the daemon owns was correct.
# Raising STAPM to 25 W took the same 8-thread load from 600 MHz to 2.7 GHz.
#
# The daemon must not exec a user-writable path (the trust argument of
# REF-REQ-111), so the binary is COPIED to a root-owned location rather than
# referenced in place. A user-writable copy would let any process running as the
# desktop user replace the program a root daemon executes.
RYZENADJ_SRC=""
for cand in "${HOME}/.local/bin/ryzenadj" "/usr/local/bin/ryzenadj" "/usr/bin/ryzenadj"; do
    if [ -x "${cand}" ]; then
        RYZENADJ_SRC="${cand}"
        break
    fi
done
if [ -n "${RYZENADJ_SRC}" ]; then
    if [ "${RYZENADJ_SRC}" != "/usr/local/bin/ryzenadj" ]; then
        ${SUDO} install -m 755 -p "${RYZENADJ_SRC}" /usr/local/bin/ryzenadj
        echo "  • ryzenadj: installed ${RYZENADJ_SRC} -> /usr/local/bin/ryzenadj (root-owned, REF-REQ-115.1)"
    else
        echo "  • ryzenadj: already at /usr/local/bin/ryzenadj"
    fi
else
    echo "  ! ryzenadj NOT found: the SMU power/thermal limits cannot be raised."
    echo "    Performance mode will be capped by the EC's own limits (REF-REQ-115.1)."
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
${SUDO} install -m 644 -p "${UNIT_SRC}" /etc/systemd/system/wattcurb.service

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
if [ -n "${TARGET_USER}" ] && [ -n "${USER_HOME}" ] && [ "${TARGET_USER}" != "root" ]; then
    if [ -d "${USER_HOME}" ]; then
        mkdir -p "${USER_HOME}/.config/autostart"
        cp /etc/xdg/autostart/wattcurb-tray.desktop "${USER_HOME}/.config/autostart/wattcurb-tray.desktop"
        chown "${TARGET_USER}:${TARGET_USER}" "${USER_HOME}/.config/autostart/wattcurb-tray.desktop" 2>/dev/null || true

        # Ensure user systemd tray unit uses /usr/local/bin
        USER_TRAY_UNIT="${USER_HOME}/.config/systemd/user/wattcurb-tray.service"
        if [ -f "${USER_TRAY_UNIT}" ]; then
            sed -i 's|ExecStart=.*wattcurb-tray|ExecStart=/usr/local/bin/wattcurb-tray|' "${USER_TRAY_UNIT}"
            chown "${TARGET_USER}:${TARGET_USER}" "${USER_TRAY_UNIT}" 2>/dev/null || true
        fi
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

# 4.4. Remove legacy per-user desktop entries (REF-REQ-121.5).
# Earlier installers wrote "${HOME}/.local/share/applications/wattcurb-dashboard.desktop"
# with "Exec=${HOME}/.local/bin/wattcurb-dashboard". A user-level entry SHADOWS the
# system one, and that path is not on the daemon's authorized-client list
# (REF-REQ-111), so every profile change made from a menu-launched dashboard was
# answered with "[ALERT:DENY] Rejected PROFILE command ... not an installed
# WattCurb client binary" and the buttons did nothing. The system entry above is
# the single source of truth for the launch path.
if [ -n "${TARGET_USER}" ] && [ "${TARGET_USER}" != "root" ]; then
    LEGACY_ENTRY="${USER_HOME}/.local/share/applications/wattcurb-dashboard.desktop"
    if [ -f "${LEGACY_ENTRY}" ]; then
        echo "  • Removing legacy user desktop entry pointing at ~/.local/bin: ${LEGACY_ENTRY}"
        rm -f "${LEGACY_ENTRY}"
    fi
fi

# 5. Launch Desktop Tray Indicator in Active Graphical Session
echo "[4/4] Starting desktop tray indicator in user graphical session..."
if [ -n "${TARGET_USER}" ] && [ "${TARGET_USER}" != "root" ]; then
    TARGET_UID=$(id -u "${TARGET_USER}" 2>/dev/null || echo "1000")
    TARGET_RUNTIME="/run/user/${TARGET_UID}"

    # Terminate any existing tray client for this user
    if [ "$(id -un 2>/dev/null || echo '')" = "${TARGET_USER}" ]; then
        pkill -f "wattcurb-tray" 2>/dev/null || true
    else
        sudo -u "${TARGET_USER}" pkill -f "wattcurb-tray" 2>/dev/null || true
    fi

    # Launch tray under the desktop user's graphical session environment
    if [ -d "${TARGET_RUNTIME}" ]; then
        # REF-REQ-122: pass the display variables too. Without them Qt aborts
        # (the tray's own "Open Dashboard" forked a binary that died with a core
        # dump). Read them from the user's systemd manager, which the desktop
        # session populates; the tray also probes the sockets itself if this
        # comes back empty.
        SESSION_ENV=""
        for var in WAYLAND_DISPLAY DISPLAY; do
            if [ "$(id -un 2>/dev/null || echo '')" = "${TARGET_USER}" ]; then
                val=$(XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
                      DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
                      systemctl --user show-environment 2>/dev/null | sed -n "s/^${var}=//p")
            else
                val=$(sudo -u "${TARGET_USER}" \
                      XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
                      DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
                      systemctl --user show-environment 2>/dev/null | sed -n "s/^${var}=//p")
            fi
            # Unquoted on purpose: these become separate VAR=value words for env.
            [ -n "${val}" ] && SESSION_ENV="${SESSION_ENV} ${var}=${val}"
        done

        # Prefer restarting through systemd user manager if the unit exists
        USER_TRAY_UNIT="${USER_HOME}/.config/systemd/user/wattcurb-tray.service"
        TRAY_STARTED=0
        if [ -f "${USER_TRAY_UNIT}" ]; then
            if [ "$(id -un 2>/dev/null || echo '')" = "${TARGET_USER}" ]; then
                systemctl --user daemon-reload 2>/dev/null || true
                if systemctl --user restart wattcurb-tray.service 2>/dev/null; then
                    TRAY_STARTED=1
                fi
            else
                sudo -u "${TARGET_USER}" \
                    XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
                    DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
                    systemctl --user daemon-reload 2>/dev/null || true
                if sudo -u "${TARGET_USER}" \
                    XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
                    DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
                    systemctl --user restart wattcurb-tray.service 2>/dev/null; then
                    TRAY_STARTED=1
                fi
            fi
        fi

        if [ "${TRAY_STARTED}" -eq 0 ]; then
            if [ "$(id -un 2>/dev/null || echo '')" = "${TARGET_USER}" ]; then
                env XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
                    DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
                    ${SESSION_ENV} \
                    nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
            else
                sudo -u "${TARGET_USER}" \
                    env XDG_RUNTIME_DIR="${TARGET_RUNTIME}" \
                    DBUS_SESSION_BUS_ADDRESS="unix:path=${TARGET_RUNTIME}/bus" \
                    ${SESSION_ENV} \
                    nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
            fi
        fi
    else
        if [ "$(id -un 2>/dev/null || echo '')" = "${TARGET_USER}" ]; then
            nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
        else
            sudo -u "${TARGET_USER}" nohup /usr/local/bin/wattcurb-tray >/dev/null 2>&1 &
        fi
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
