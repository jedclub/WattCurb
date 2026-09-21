# REF-ARCH-066: Matrix Dashboard Two-Column Legend Architecture & Proportional Font Scaling

- **Document ID**: `REF-ARCH-066`
- **Related Requirements**: [`REF-REQ-089`](../requirements/REQ-089-matrix-dashboard-expanded-power-shares-and-typography.md)
- **Related Architecture**: [`REF-ARCH-037`](ARCH-037-grid-aligned-dual-domain-ui-architecture.md), [`REF-ARCH-038`](ARCH-038-proportional-ui-typography-and-grid-scaling.md), [`REF-ARCH-051`](ARCH-051-dashboard-matrix-profiling-scopes-and-zero-copy-ingestion.md)
- **Status**: Approved
- **Date**: 2026-09-21

---

## 1. Architectural Overview

```mermaid
flowchart TD
    subgraph Lower Power Share Deck [Lower Power Share Analytics Deck: 190px Preferred Height]
        direction LR
        subgraph CardA [Card A: Hardware Devices (460px)]
            DonutA["Donut Canvas (110x110)"]
            GridA["2-Column GridLayout (columns: 2)
            Col 1: CPU, GPU, Display, NVMe, Fan
            Col 2: DRAM, VRM, Wi-Fi, Motherboard"]
        end

        subgraph CardB [Card B: Processes Attribution (fillWidth)]
            DonutB["Donut Canvas (110x110)"]
            GridB["2-Column GridLayout (columns: 2)
            Col 1: Top 1..6 Processes
            Col 2: Top 7..11 Processes + Other"]
        end
    end
```

---

## 2. Component Design & Layout Restructuring

### 2.1 Card A: Hardware Devices Two-Column Grid
```qml
// Legend 2-Column Grid for Hardware Devices (All items fully visible!)
GridLayout {
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.alignment: Qt.AlignVCenter
    columns: 2
    columnSpacing: 10
    rowSpacing: 3

    Repeater {
        model: backend.devicePowerShares
        delegate: RowLayout {
            Layout.fillWidth: true
            spacing: 5

            Rectangle {
                width: 8; height: 8; radius: 2
                color: modelData.color
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: modelData.name
                color: root.textMain
                font.pixelSize: 12
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Text {
                text: modelData.watts.toFixed(1) + "W"
                color: modelData.color
                font.pixelSize: 12
                font.family: "Monospace"
            }
            Text {
                text: modelData.pct.toFixed(0) + "%"
                color: root.textDim
                font.pixelSize: 12
                font.family: "Monospace"
                Layout.preferredWidth: 32
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
```

### 2.2 Card B: Top 11 Processes 2-Column Expansion
In `src/ui/dashboard_backend.cpp`:
```cpp
const QString proc_colors[] = {
    QStringLiteral("#ef4444"), // Red (Top 1)
    QStringLiteral("#f97316"), // Orange (Top 2)
    QStringLiteral("#f59e0b"), // Amber (Top 3)
    QStringLiteral("#eab308"), // Yellow (Top 4)
    QStringLiteral("#10b981"), // Emerald (Top 5)
    QStringLiteral("#14b8a6"), // Teal (Top 6)
    QStringLiteral("#00d2ff"), // Cyan (Top 7)
    QStringLiteral("#3b82f6"), // Blue (Top 8)
    QStringLiteral("#6366f1"), // Indigo (Top 9)
    QStringLiteral("#a855f7"), // Purple (Top 10)
    QStringLiteral("#ec4899")  // Pink (Top 11)
};

size_t count = std::min<size_t>(11, cached_proc_summaries_.size());
```

---

## 3. Global Typography Scale Mapping

| UI Section | Old Font Size | New Font Size | Visual Benefit |
| :--- | :--- | :--- | :--- |
| **Window Title & Top Brand** | 16px | 18px | Stronger application branding |
| **System Total Power (Header)** | 22px | 24px | Immediate glanceability from a distance |
| **Process Table Headers** | 11px / 12px | 12px / 13px | Clear column distinction |
| **Process Table Row Items** | 11px / 12px | 12px / 13px | Reduced eye strain during audit |
| **Process Table Row Height** | 30px | 33px | Enhanced vertical breathing room |
| **Hardware Cards (CPU/GPU/Bat)** | 11px / 12px | 12px / 13px | Readability on high-DPI displays |
| **Power Share Deck Headers** | 13px / 14px | 15px / 16px | Symmetrical deck visual weight |
| **Power Share Deck Legend Items** | 11px | 12px | Crisp text without horizontal clipping |
| **Bottom Control Dock Buttons** | 11px (30px H) | 12px (32px H) | Tactile touch & click ergonomics |

---

## 4. Verification & Oracle Gate Standards (`REF-TEST-053`)

- `test_matrix_dashboard_expanded_power_shares_and_typography()`:
  - Asserts `device_power_shares_` contains all decomposed domains (>= 8 domains when platform loss exists).
  - Asserts `process_power_shares_` supports up to 12 items (Top 11 + Other).
  - Validates QML source integrity: ensures Card A has `columns: 2`, Card B utilizes 11-process model, deck height is 190px, and font pixel sizes are >= 12px for body content.
