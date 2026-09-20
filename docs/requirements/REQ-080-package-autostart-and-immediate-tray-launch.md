# REF-REQ-080: Package Autostart & Immediate Graphical Session Tray Launch

## 1. Overview & Problem Statement

When deploying WattCurb via release distribution archives (`wattcurb-${TAG}-linux-x86_64.tar.gz`) or system package installers:
1. The background profiling engine (`wattcurb`) requires native Administrator (root) privileges to interface with hardware RAPL powercap, NVMe APST, and PCIe ASPM interfaces.
2. The desktop tray indicator (`wattcurb-tray`) and matrix dashboard (`wattcurb-dashboard`) are Qt6/Wayland/X11 graphical applications that must run within the active user's graphical desktop session (connecting to `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS`).
3. Running `sudo ./install.sh` traditionally executed GUI launchers as `root`, causing immediate process failure due to missing graphical session buses, and placed autostart desktop entries in `/root/.config/autostart/`, leaving regular user sessions unpopulated upon subsequent logins.

This requirement defines the specifications for universal system-level autostart and immediate graphical session tray launching upon installation.

---

## 2. Functional Requirements

### 2.1 Daemon Root Service Autostart
- The installer MUST deploy the daemon unit to `/etc/systemd/system/wattcurb.service`.
- The installer MUST immediately reload systemd (`systemctl daemon-reload`) and enable/start the service (`systemctl enable --now wattcurb.service`).
- The daemon MUST begin hardware power profiling immediately upon package installation without requiring a system reboot.

### 2.2 System-Wide Desktop Autostart (`/etc/xdg/autostart`)
- The installer MUST deploy `wattcurb-tray.desktop` to `/etc/xdg/autostart/wattcurb-tray.desktop`.
- System-wide XDG autostart ensures that *any* desktop user logging into KDE Plasma, GNOME, XFCE, or other compliant environments will automatically launch `wattcurb-tray`.
- As a fail-safe, if `SUDO_USER` is identified, a mirrored copy MUST be synchronized to `${USER_HOME}/.config/autostart/wattcurb-tray.desktop` with correct user ownership.

### 2.3 Immediate Graphical Session Tray Launch
- Upon installation completion, the installer MUST NOT launch `wattcurb-tray` as `root`.
- The installer MUST resolve the active graphical user session:
  1. Identify `TARGET_USER` via `${SUDO_USER:-$USER}`.
  2. Resolve user UID via `id -u "${TARGET_USER}"`.
  3. Verify runtime directory `/run/user/${TARGET_UID}` and session bus socket `/run/user/${TARGET_UID}/bus`.
- The installer MUST launch `wattcurb-tray` via `sudo -u "${TARGET_USER}"` with injected `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS`.
- The StatusNotifierItem (SNI) tray icon MUST become visible on the host desktop panel immediately upon installation completion.

### 2.4 Desktop Application Entry
- The installer MUST deploy `/usr/share/applications/wattcurb-dashboard.desktop` so users can launch the Matrix Dashboard from standard desktop application menus.

### 2.5 Clean Uninstallation
- `uninstall.sh` MUST purge all systemd units, `/etc/xdg/autostart/wattcurb-tray.desktop`, `/usr/share/applications/wattcurb-dashboard.desktop`, user autostart entries, binaries, and shared memory segments (`/dev/shm/wattcurb_*.shm`).
