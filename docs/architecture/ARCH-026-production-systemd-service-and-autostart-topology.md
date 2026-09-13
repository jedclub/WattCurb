# ARCH-026: Production Systemd Service & Autostart Topology

## 1. Overview & Context

- **REF-ID**: `REF-ARCH-026`
- **Related Architecture**: [`REF-ARCH-001`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-001-system-overview.md), [`REF-ARCH-008`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-008-zero-copy-binary-ipc-and-memory-sequence-profiler.md), [`REF-ARCH-025`](file:///home/jedclub/Develop/WattCurb/docs/architecture/ARCH-025-zero-alloc-sni-tray-client.md)
- **Target OS**: Linux (Arch / CachyOS, KDE Plasma 6.7 Wayland, systemd user session)

---

## 2. Dual-Service Topology Architecture

```mermaid
flowchart TD
    subgraph systemd_user["systemd User Session (UID 1000)"]
        GTarget["graphical-session.target"]
        
        subgraph backend["WattCurb Daemon Service"]
            WServ["wattcurb.service"] -->|ExecStart| WBin["~/.local/bin/wattcurb --daemon"]
            WBin -->|Writes (Lock-free)| SHM["/dev/shm/wattcurb_state.shm (128B Seqlock)"]
            WBin -->|Listens (Non-blocking)| USock["/tmp/wattcurb_lock.sock (UDS Control)"]
        end

        subgraph frontend["WattCurb Tray Service"]
            TServ["wattcurb-tray.service"] -->|ExecStart| TBin["~/.local/bin/wattcurb-tray"]
            TBin -->|Reads (35ns)| SHM
            TBin -->|Commands| USock
            TBin -->|Registers SNI| DBus["Session D-Bus (org.kde.StatusNotifierItem)"]
        end

        GTarget --> WServ
        GTarget --> TServ
        TServ -.->|After & Wants| WServ
    end

    DBus --> KDEPanel["KDE Plasma System Tray"]
```

---

## 3. Unit Definitions

### 3.1. `~/.config/systemd/user/wattcurb.service`
```ini
[Unit]
Description=WattCurb Ultra-Low-Overhead Power Profiling & Mitigation Daemon
Documentation=https://github.com/jedclub/WattCurb
After=graphical-session.target
PartOf=graphical-session.target

[Service]
Type=simple
ExecStart=%h/.local/bin/wattcurb --daemon
Restart=on-failure
RestartSec=3
KillMode=mixed
TimeoutStopSec=2

[Install]
WantedBy=graphical-session.target
```

### 3.2. `~/.config/systemd/user/wattcurb-tray.service`
```ini
[Unit]
Description=WattCurb Desktop StatusNotifierItem Tray Indicator
Documentation=https://github.com/jedclub/WattCurb
After=graphical-session.target wattcurb.service
Wants=wattcurb.service
PartOf=graphical-session.target

[Service]
Type=simple
ExecStart=%h/.local/bin/wattcurb-tray
Restart=on-failure
RestartSec=2
KillMode=mixed
TimeoutStopSec=1

[Install]
WantedBy=graphical-session.target
```

### 3.3. `~/.config/autostart/wattcurb-tray.desktop`
```ini
[Desktop Entry]
Name=WattCurb Tray
Comment=WattCurb Ultra-Low-Overhead Power Indicator
Exec=/home/jedclub/.local/bin/wattcurb-tray
Icon=battery-good
Terminal=false
Type=Application
Categories=Utility;System;
StartupNotify=false
X-GNOME-Autostart-enabled=true
X-KDE-autostart-after=panel
X-systemd-skip=true
```

---

## 4. Lifecycle & Failure Recovery

1. **Daemon Crash**:
   - `wattcurb-tray` detects stale SHM data or missed socket acks without crashing.
   - `wattcurb.service` automatically restarts in 3 seconds.
   - Once restarted, `wattcurb` re-initializes `/dev/shm/wattcurb_state.shm` atomically.
2. **Tray Crash**:
   - `wattcurb-tray.service` restarts in 2 seconds.
   - Immediately attaches to existing `/dev/shm/wattcurb_state.shm` and restores tray icon.
