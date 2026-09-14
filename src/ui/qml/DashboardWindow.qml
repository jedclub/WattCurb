import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1160
    height: 720
    minimumWidth: 980
    minimumHeight: 620
    title: "WattCurb btop-Style Power & Hardware Matrix Dashboard"
    color: "#0b0e12"

    // btop cyber terminal palette
    readonly property color bgApp: "#0b0e12"
    readonly property color bgPanel: "#12161d"
    readonly property color bgPanelHeader: "#181e26"
    readonly property color borderPanel: "#222a36"
    readonly property color bgRowAlt: "#151922"
    readonly property color textMain: "#e5e7eb"
    readonly property color textDim: "#8892a0"
    readonly property color textMuted: "#5b6574"
    
    readonly property color colCyan: "#00d2ff"
    readonly property color colGreen: "#10b981"
    readonly property color colOrange: "#f59e0b"
    readonly property color colRed: "#ef4444"
    readonly property color colPurple: "#a855f7"
    readonly property color colBlue: "#3b82f6"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // =============================================================
        // 1. TOP DENSE HEADER BAR (34px)
        // =============================================================
        Rectangle {
            Layout.fillWidth: true
            height: 34
            color: root.bgPanel
            border.color: root.borderPanel
            radius: 5

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10

                // App Title
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 20; height: 20; radius: 4; color: "#0284c7"
                        Text { anchors.centerIn: parent; text: "⚡"; font.pixelSize: 11 }
                    }
                    Text {
                        text: "WATTCURB"
                        color: root.colCyan
                        font.bold: true
                        font.pixelSize: 12
                        font.family: "Monospace"
                    }
                    Text {
                        text: "[btop Hardware & Process Power Matrix]"
                        color: root.textDim
                        font.pixelSize: 10
                    }
                }

                Item { Layout.fillWidth: true }

                // Total Discharge
                RowLayout {
                    spacing: 4
                    Text { text: "TOTAL DRAIN:"; color: root.textDim; font.pixelSize: 10; font.bold: true }
                    Text {
                        text: backend.systemDrainWatts.toFixed(2) + " W"
                        color: backend.systemDrainWatts > 20.0 ? root.colRed : (backend.systemDrainWatts > 12.0 ? root.colOrange : root.colCyan)
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "Monospace"
                    }
                }

                // Battery Status Tag
                Rectangle {
                    height: 20
                    width: batTagRow.width + 10
                    radius: 3
                    color: backend.batteryState === 1 ? "#341717" : "#122a1f"
                    border.color: backend.batteryState === 1 ? root.colOrange : root.colGreen

                    RowLayout {
                        id: batTagRow
                        anchors.centerIn: parent
                        spacing: 4
                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: backend.batteryState === 1 ? root.colOrange : root.colGreen
                        }
                        Text {
                            text: backend.batteryPercent + "% (" + backend.batteryStateString + ")"
                            color: backend.batteryState === 1 ? root.colOrange : root.colGreen
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                // Profile Tag
                Rectangle {
                    height: 20
                    width: profTagText.width + 10
                    radius: 3
                    color: "#1e2430"
                    border.color: root.borderPanel
                    Text {
                        id: profTagText
                        anchors.centerIn: parent
                        text: backend.powerProfileName
                        color: root.textMain
                        font.pixelSize: 9
                    }
                }

                // Clock
                Text {
                    text: "SYNC " + backend.lastUpdateTime
                    color: root.textMuted
                    font.pixelSize: 9
                    font.family: "Monospace"
                }
            }
        }

        // =============================================================
        // 2. MAIN 2-COLUMN VIEW (LEFT: HARDWARE, RIGHT: PROCESS TABLE)
        // =============================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            // ---------------------------------------------------------
            // LEFT COLUMN: DENSE HARDWARE METRICS (Strict 410px Width)
            // ---------------------------------------------------------
            ColumnLayout {
                Layout.preferredWidth: 410
                Layout.minimumWidth: 410
                Layout.maximumWidth: 410
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: 6

                // CARD 1: CPU & Memory Subsystem (RAPL)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 185
                    color: root.bgPanel
                    border.color: root.borderPanel
                    radius: 5
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        // Title
                        RowLayout {
                            Text { text: "💻 CPU & MEMORY SUBSYSTEM (RAPL)"; color: root.colCyan; font.bold: true; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.cpuDrainWatts.toFixed(2) + " W"; color: root.colCyan; font.bold: true; font.pixelSize: 12; font.family: "Monospace" }
                        }

                        // Detailed Breakdowns
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Pkg: " + backend.cpuDrainWatts.toFixed(2) + "W"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
                            Text { text: "| Core: " + backend.cpuCoreWatts.toFixed(2) + "W"; color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
                            Text { text: "| Uncore: " + backend.cpuUncoreWatts.toFixed(2) + "W"; color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
                            Text { text: "| DRAM: " + backend.cpuDramWatts.toFixed(2) + "W"; color: root.colPurple; font.pixelSize: 9; font.family: "Monospace" }
                        }

                        // Temp, Frequency, Fan
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: "Core Temp: " + backend.cpuTempC + "°C"
                                color: backend.cpuTempC > 75 ? root.colRed : (backend.cpuTempC > 60 ? root.colOrange : root.colGreen)
                                font.pixelSize: 9; font.bold: true
                            }
                            Text { text: "Freq: " + backend.cpuFreqMhz + " MHz"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
                            Text { text: "Fan: " + backend.fanRpm + " RPM"; color: root.textDim; font.pixelSize: 9 }
                        }

                        // C-State Sleep Residencies
                        Text { text: "C-State Sleep Residencies:"; color: root.textDim; font.pixelSize: 9; font.bold: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            // C0 (Active)
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 1
                                RowLayout {
                                    Text { text: "C0"; color: root.textDim; font.pixelSize: 8 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC0Percent.toFixed(1) + "%"; color: root.textMain; font.pixelSize: 8; font.family: "Monospace" }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 4; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC0Percent / 100.0); height: parent.height; radius: 2; color: root.colOrange }
                                }
                            }

                            // C1
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 1
                                RowLayout {
                                    Text { text: "C1"; color: root.textDim; font.pixelSize: 8 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC1Percent.toFixed(1) + "%"; color: root.textMain; font.pixelSize: 8; font.family: "Monospace" }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 4; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC1Percent / 100.0); height: parent.height; radius: 2; color: root.colBlue }
                                }
                            }

                            // C2
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 1
                                RowLayout {
                                    Text { text: "C2"; color: root.textDim; font.pixelSize: 8 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC2Percent.toFixed(1) + "%"; color: root.textMain; font.pixelSize: 8; font.family: "Monospace" }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 4; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC2Percent / 100.0); height: parent.height; radius: 2; color: root.colBlue }
                                }
                            }

                            // C3 (Deep Sleep)
                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 1
                                RowLayout {
                                    Text { text: "C3 (Deep)"; color: root.colGreen; font.pixelSize: 8; font.bold: true }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC3Percent.toFixed(1) + "%"; color: root.colGreen; font.pixelSize: 8; font.family: "Monospace"; font.bold: true }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 4; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC3Percent / 100.0); height: parent.height; radius: 2; color: root.colGreen }
                                }
                            }
                        }

                        Text {
                            text: "Zero-Wakeup Guard: " + backend.wakeupsPerSec + " interrupts/s | " + backend.activeMitigations + " Protections Active"
                            color: root.textMuted
                            font.pixelSize: 8
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                // CARD 2: Battery & Electrical Telemetry
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 145
                    color: root.bgPanel
                    border.color: root.borderPanel
                    radius: 5
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        RowLayout {
                            Text { text: "🔋 BATTERY & POWER SUPPLY (BAT0)"; color: root.colGreen; font.bold: true; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.batteryPercent + "%"; color: root.colGreen; font.bold: true; font.pixelSize: 13; font.family: "Monospace" }
                        }

                        // Battery Bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 5; radius: 2; color: "#222a36"
                            Rectangle {
                                width: parent.width * Math.min(1.0, backend.batteryPercent / 100.0)
                                height: parent.height; radius: 2
                                color: backend.batteryPercent < 20 ? root.colRed : (backend.batteryPercent < 45 ? root.colOrange : root.colGreen)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Volt: " + backend.batteryVoltageV.toFixed(2) + " V"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
                            Item { Layout.fillWidth: true }
                            Text { text: "Curr: " + backend.batteryCurrentA.toFixed(2) + " A"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
                            Item { Layout.fillWidth: true }
                            Text { text: "Cycles: " + backend.batteryCycles; color: root.textDim; font.pixelSize: 9 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Health: " + backend.batteryHealth + "% (SMP Li-poly)"; color: root.colGreen; font.pixelSize: 9; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: "Time to Empty: " + backend.timeToEmptyString; color: root.textMain; font.pixelSize: 9; font.bold: true }
                        }

                        Text {
                            text: backend.batteryState === 2 ? "AC Hardware Pass-through Active (Zero wear)" : "Adaptive Power Optimization Active"
                            color: root.textMuted
                            font.pixelSize: 8
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                // CARD 3: GPU, Display & Storage Domains
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.bgPanel
                    border.color: root.borderPanel
                    radius: 5
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Text { text: "🎮 GPU, 🖥️ DISPLAY & 💾 STORAGE"; color: root.colOrange; font.bold: true; font.pixelSize: 10 }

                        // GPU
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "GPU Silicon (iGPU):"; color: root.textMain; font.pixelSize: 9; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.gpuDrainWatts.toFixed(2) + " W"; color: root.colGreen; font.pixelSize: 10; font.bold: true; font.family: "Monospace" }
                            Text { text: "(Load: " + backend.gpuLoadPercent + "%)"; color: root.textDim; font.pixelSize: 9 }
                        }
                        Text { text: "AMD Radeon 780M / Dynamic Power-Gating"; color: root.textMuted; font.pixelSize: 8 }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#19202a" }

                        // Display
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Display & Backlight:"; color: root.textMain; font.pixelSize: 9; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.displayDrainWatts.toFixed(2) + " W"; color: root.colOrange; font.pixelSize: 10; font.bold: true; font.family: "Monospace" }
                            Text { text: "(" + backend.displayBrightnessPct + "% bright)"; color: root.textDim; font.pixelSize: 9 }
                        }
                        Text { text: "amdgpu_bl1 Adaptive Dynamic Dimming & VRR Ready"; color: root.textMuted; font.pixelSize: 8 }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#19202a" }

                        // Storage
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "Storage & NVMe SSD:"; color: root.textMain; font.pixelSize: 9; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.nvmeDrainWatts.toFixed(2) + " W"; color: root.colCyan; font.pixelSize: 10; font.bold: true; font.family: "Monospace" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "APST L1.2 Ultra-Low Sleep"; color: root.colGreen; font.pixelSize: 8 }
                            Item { Layout.fillWidth: true }
                            Text { text: "R: " + backend.diskReadMbPerSec.toFixed(1) + "M | W: " + backend.diskWriteMbPerSec.toFixed(1) + "MB/s"; color: root.textDim; font.pixelSize: 8; font.family: "Monospace" }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // RIGHT COLUMN: btop PROCESS POWER MATRIX TABLE (Fills Remainder)
            // ---------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.minimumWidth: 520
                Layout.fillHeight: true
                color: root.bgPanel
                border.color: root.borderPanel
                radius: 5
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    // Table Header Bar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: "⚡ TOP PROCESS ATTRIBUTION MATRIX (btop Power Monitor)"
                            color: root.colCyan
                            font.bold: true
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: (backend.processList.length > 0 ? backend.processList.length : 0) + " Processes Analyzed"
                            color: root.textDim
                            font.pixelSize: 9
                        }
                    }

                    // Table Column Headers
                    Rectangle {
                        Layout.fillWidth: true
                        height: 22
                        color: root.bgPanelHeader
                        radius: 3

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 4

                            Text { text: "PID"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 42; clip: true }
                            Text { text: "PROGRAM"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 105; clip: true }
                            Text { text: "TOTAL"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 55; clip: true }
                            Text { text: "RATIO"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 55; clip: true }
                            Text { text: "CPU/GPU/DRAM"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 95; clip: true }
                            Text { text: "PSS"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 42; clip: true }
                            Text { text: "TIER"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.preferredWidth: 50; clip: true }
                            Text { text: "PRIMARY HARDWARE MECHANISM"; color: root.textDim; font.pixelSize: 9; font.bold: true; Layout.fillWidth: true; clip: true }
                        }
                    }

                    // Process ListView
                    ListView {
                        id: procListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: backend.processList
                        spacing: 2

                        delegate: Rectangle {
                            width: procListView.width
                            height: 26
                            radius: 3
                            color: rowMa.containsMouse ? "#1f2633" : (index % 2 === 0 ? root.bgRowAlt : root.bgPanel)

                            MouseArea {
                                id: rowMa
                                anchors.fill: parent
                                hoverEnabled: true
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 4

                                // PID
                                Text {
                                    text: modelData["pid"] !== undefined ? modelData["pid"] : ""
                                    color: root.textDim
                                    font.pixelSize: 9
                                    font.family: "Monospace"
                                    Layout.preferredWidth: 42
                                    elide: Text.ElideRight
                                    clip: true
                                }

                                // Comm / Program
                                Text {
                                    text: modelData["comm"] !== undefined ? modelData["comm"] : ""
                                    color: root.textMain
                                    font.bold: true
                                    font.pixelSize: 10
                                    Layout.preferredWidth: 105
                                    elide: Text.ElideRight
                                    clip: true
                                }

                                // Total Watts / mW
                                Text {
                                    readonly property real w: modelData["totalWatts"] !== undefined ? modelData["totalWatts"] : 0.0
                                    text: w >= 1.0 ? (w.toFixed(2) + "W") : ((w * 1000).toFixed(0) + "m")
                                    color: w > 1.5 ? root.colRed : (w > 0.5 ? root.colOrange : root.colCyan)
                                    font.bold: true
                                    font.pixelSize: 9
                                    font.family: "Monospace"
                                    Layout.preferredWidth: 55
                                    elide: Text.ElideRight
                                    clip: true
                                }

                                // Ratio Mini Bar
                                RowLayout {
                                    Layout.preferredWidth: 55
                                    spacing: 2
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 4; radius: 2; color: "#222a36"
                                        Rectangle {
                                            readonly property real r: modelData["ratioPercent"] !== undefined ? modelData["ratioPercent"] : 0.0
                                            width: parent.width * Math.min(1.0, r / 100.0)
                                            height: parent.height; radius: 2
                                            color: r > 10.0 ? root.colOrange : root.colCyan
                                        }
                                    }
                                    Text {
                                        readonly property real r: modelData["ratioPercent"] !== undefined ? modelData["ratioPercent"] : 0.0
                                        text: r.toFixed(0) + "%"
                                        color: root.textDim
                                        font.pixelSize: 8
                                        font.family: "Monospace"
                                    }
                                }

                                // Breakdown: CPU / GPU / DRAM
                                Text {
                                    readonly property real c: modelData["cpuWatts"] !== undefined ? modelData["cpuWatts"] : 0.0
                                    readonly property real g: modelData["gpuWatts"] !== undefined ? modelData["gpuWatts"] : 0.0
                                    readonly property real d: modelData["dramWatts"] !== undefined ? modelData["dramWatts"] : 0.0
                                    text: (c * 1000).toFixed(0) + "/" + (g * 1000).toFixed(0) + "/" + (d * 1000).toFixed(0)
                                    color: root.textDim
                                    font.pixelSize: 8
                                    font.family: "Monospace"
                                    Layout.preferredWidth: 95
                                    elide: Text.ElideRight
                                    clip: true
                                }

                                // PSS (Memory)
                                Text {
                                    text: (modelData["pssMb"] !== undefined ? modelData["pssMb"] : 0) + "M"
                                    color: root.textMain
                                    font.pixelSize: 9
                                    font.family: "Monospace"
                                    Layout.preferredWidth: 42
                                    elide: Text.ElideRight
                                    clip: true
                                }

                                // Safety Tier Badge
                                Rectangle {
                                    Layout.preferredWidth: 50
                                    height: 16
                                    radius: 2
                                    readonly property int t: modelData["tier"] !== undefined ? modelData["tier"] : 0
                                    color: t === 0 ? "#142921" : (t === 5 ? "#361313" : "#1b222d")
                                    border.color: t === 0 ? root.colGreen : (t === 5 ? root.colRed : root.borderPanel)
                                    Text {
                                        anchors.centerIn: parent
                                        text: "Tier " + parent.t
                                        color: parent.t === 0 ? root.colGreen : (parent.t === 5 ? root.colRed : root.textDim)
                                        font.pixelSize: 8
                                        font.bold: true
                                    }
                                }

                                // Primary Hardware Mechanism
                                Text {
                                    readonly property string dom: modelData["domain"] !== undefined ? modelData["domain"] : ""
                                    readonly property string mech: modelData["mechanism"] !== undefined ? modelData["mechanism"] : ""
                                    text: "[" + dom + "] " + mech
                                    color: root.textDim
                                    font.pixelSize: 8
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    clip: true
                                }
                            }
                        }
                    }

                    // Table Footer
                    Text {
                        text: "* CPU/GPU/DRAM: Attributed Silicon Watts (mW) | PSS: Proportional Set Size RAM | Tier 0: Kernel/Critical ~ Tier 5: Runaway"
                        color: root.textMuted
                        font.pixelSize: 8
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // =============================================================
        // 3. BOTTOM CONTROL BAR (38px)
        // =============================================================
        Rectangle {
            Layout.fillWidth: true
            height: 38
            color: root.bgPanel
            border.color: root.borderPanel
            radius: 5

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text { text: "PROFILES:"; color: root.textDim; font.bold: true; font.pixelSize: 9 }

                // 4 Compact Profile Buttons
                Repeater {
                    model: [
                        { mode: 0, label: "🚀 Performance (4.1G)" },
                        { mode: 1, label: "⚖️ Balanced" },
                        { mode: 2, label: "🍃 Smart Save (1.7G)" },
                        { mode: 3, label: "❄️ Ultra Save (1.4G/48Hz)" }
                    ]

                    Rectangle {
                        property bool isSelected: backend.powerProfileMode === modelData.mode
                        width: 140
                        height: 26
                        radius: 3
                        color: isSelected ? "#0284c7" : "#1c222c"
                        border.color: isSelected ? "#38bdf8" : "#2d3748"
                        border.width: isSelected ? 1.5 : 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: isSelected ? "#ffffff" : root.textMain
                            font.bold: isSelected
                            font.pixelSize: 9
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.setProfile(modelData.mode)
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Rescan Button
                Rectangle {
                    width: 130
                    height: 26
                    radius: 3
                    color: backend.isRescanning ? "#2d3748" : "#059669"
                    border.color: backend.isRescanning ? "#4a5568" : "#10b981"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: backend.isRescanning ? "⏳ 측정 중..." : "🔄 지금 정밀 재측정"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 9
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: backend.isRescanning ? Qt.ArrowCursor : Qt.PointingHandCursor
                        enabled: !backend.isRescanning
                        onClicked: backend.triggerRescan()
                    }
                }

                // System Monitor
                Rectangle {
                    width: 110
                    height: 26
                    radius: 3
                    color: "#1c222c"
                    border.color: "#2d3748"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 3
                        Text { text: "📊 시스템 모니터"; color: root.textMain; font.pixelSize: 9 }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.openSystemMonitor()
                    }
                }

                // Close Button
                Rectangle {
                    width: 26
                    height: 26
                    radius: 3
                    color: "#1c222c"
                    border.color: "#2d3748"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: root.textDim
                        font.bold: true
                        font.pixelSize: 10
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }
            }
        }
    }
}
