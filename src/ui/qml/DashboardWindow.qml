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

    // Hover & Pinned Deep Inspection State (REF-REQ-038)
    property bool hoverVisible: false
    property bool hoverPinned: false
    property string hoverType: "" // "process", "cpu", "battery", "gpu"
    property var hoverData: null
    property real hoverTargetX: 0
    property real hoverTargetY: 0

    Shortcut {
        sequence: "Escape"
        onActivated: {
            root.hoverPinned = false;
            root.hoverVisible = false;
        }
    }

    // Initial inspection for top runaway process (REF-REQ-038)
    Timer {
        id: autoInspectTimer
        interval: 350
        running: true
        repeat: false
        onTriggered: {
            if (backend.processList.length > 0 && !root.hoverVisible) {
                root.hoverType = "process";
                root.hoverData = backend.processList[0];
                root.hoverTargetX = root.width * 0.52;
                root.hoverTargetY = 220;
                root.hoverVisible = true;
                root.hoverPinned = true;
            }
        }
    }

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
                        text: "[btop Deep Hardware & Process Power Matrix]"
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
                    color: cpuMa.containsMouse ? "#161b24" : root.bgPanel
                    border.color: cpuMa.containsMouse ? root.colCyan : root.borderPanel
                    radius: 5
                    clip: true

                    MouseArea {
                        id: cpuMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.hoverPinned && root.hoverType === "cpu") {
                                root.hoverPinned = false;
                                root.hoverVisible = false;
                            } else {
                                var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                root.hoverTargetX = pos.x;
                                root.hoverTargetY = pos.y;
                                root.hoverType = "cpu";
                                root.hoverVisible = true;
                                root.hoverPinned = true;
                            }
                        }
                        onEntered: {
                            if (!root.hoverPinned) {
                                var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                root.hoverTargetX = pos.x;
                                root.hoverTargetY = pos.y;
                                root.hoverType = "cpu";
                                root.hoverVisible = true;
                            }
                        }
                        onPositionChanged: {
                            if (!root.hoverPinned) {
                                var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                root.hoverTargetX = pos.x;
                                root.hoverTargetY = pos.y;
                            }
                        }
                        onExited: {
                            if (!root.hoverPinned) {
                                root.hoverVisible = false;
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        // Title
                        RowLayout {
                            Text { text: "💻 CPU & MEMORY SUBSYSTEM (RAPL)"; color: root.colCyan; font.bold: true; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                            Text { text: "🔍 호버 상세" ; color: root.textMuted; font.pixelSize: 8 }
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
                            Text { text: "Freq: " + backend.cpuFreqMhz + " MHz (" + backend.cpuGovernor + ")"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
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
                            text: "PMU IPC: " + backend.pmuIpc.toFixed(2) + " | Waste Ratio: " + backend.pmuEwr.toFixed(1) + "% | " + backend.wakeupsPerSec + " wakeups/s"
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
                    color: batMa.containsMouse ? "#161b24" : root.bgPanel
                    border.color: batMa.containsMouse ? root.colGreen : root.borderPanel
                    radius: 5
                    clip: true

                    MouseArea {
                        id: batMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.hoverPinned && root.hoverType === "battery") {
                                root.hoverPinned = false;
                                root.hoverVisible = false;
                            } else {
                                var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                root.hoverTargetX = pos.x;
                                root.hoverTargetY = pos.y;
                                root.hoverType = "battery";
                                root.hoverVisible = true;
                                root.hoverPinned = true;
                            }
                        }
                        onEntered: {
                            if (!root.hoverPinned) {
                                var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                root.hoverTargetX = pos.x;
                                root.hoverTargetY = pos.y;
                                root.hoverType = "battery";
                                root.hoverVisible = true;
                            }
                        }
                        onPositionChanged: {
                            if (!root.hoverPinned) {
                                var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                root.hoverTargetX = pos.x;
                                root.hoverTargetY = pos.y;
                            }
                        }
                        onExited: {
                            if (!root.hoverPinned) {
                                root.hoverVisible = false;
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        RowLayout {
                            Text { text: "🔋 BATTERY & POWER SUPPLY (BAT0)"; color: root.colGreen; font.bold: true; font.pixelSize: 10 }
                            Item { Layout.fillWidth: true }
                            Text { text: "🔍 호버 상세" ; color: root.textMuted; font.pixelSize: 8 }
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
                            Text { text: "Health: " + backend.batteryHealth + "% (" + backend.batteryTech + ")"; color: root.colGreen; font.pixelSize: 9; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: "Time: " + backend.timeToEmptyString; color: root.textMain; font.pixelSize: 9; font.bold: true }
                        }

                        Text {
                            text: backend.batteryState === 2 ? "AC Hardware Pass-through Active (Zero wear)" : "Adaptive Power Optimization Active (" + backend.batteryModel + ")"
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
                            Text { text: "APST L1.2 Ultra-Low Sleep (" + backend.aspmPolicy + ")"; color: root.colGreen; font.pixelSize: 8 }
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
                            text: "💡 행에 마우스를 올리면 전력/스케줄러/메모리 심층 분석 카드 표시"
                            color: root.colOrange
                            font.pixelSize: 8
                            font.bold: true
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
                            id: rowDelegate
                            width: procListView.width
                            height: 26
                            radius: 3
                            color: rowMa.containsMouse ? "#202838" : (index % 2 === 0 ? root.bgRowAlt : root.bgPanel)

                            MouseArea {
                                id: rowMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.hoverPinned && root.hoverType === "process" && root.hoverData === modelData) {
                                        root.hoverPinned = false;
                                        root.hoverVisible = false;
                                    } else {
                                        var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                        root.hoverTargetX = pos.x;
                                        root.hoverTargetY = pos.y;
                                        root.hoverType = "process";
                                        root.hoverData = modelData;
                                        root.hoverVisible = true;
                                        root.hoverPinned = true;
                                    }
                                }
                                onEntered: {
                                    if (!root.hoverPinned) {
                                        var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                        root.hoverTargetX = pos.x;
                                        root.hoverTargetY = pos.y;
                                        root.hoverType = "process";
                                        root.hoverData = modelData;
                                        root.hoverVisible = true;
                                    }
                                }
                                onPositionChanged: {
                                    if (!root.hoverPinned) {
                                        var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                        root.hoverTargetX = pos.x;
                                        root.hoverTargetY = pos.y;
                                    }
                                }
                                onExited: {
                                    if (!root.hoverPinned) {
                                        root.hoverVisible = false;
                                    }
                                }
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

    // =============================================================
    // 4. FLOATING CYBER INSPECTION CARD (REF-REQ-038)
    // =============================================================
    Rectangle {
        id: hoverCard
        visible: root.hoverVisible
        opacity: root.hoverVisible ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 120 } }
        z: 9999

        // Dimensions
        width: root.hoverType === "process" ? 440 : 380
        height: root.hoverType === "process" ? 300 : 210

        // Smart Edge Clamping: Ensure hover card never overflows the window borders
        x: Math.min(root.width - width - 12, Math.max(12, root.hoverTargetX - (root.hoverTargetX > root.width * 0.6 ? (width + 10) : -15)))
        y: Math.min(root.height - height - 12, Math.max(12, root.hoverTargetY - (root.hoverTargetY > root.height * 0.6 ? (height + 10) : -15)))

        // Modern Glassmorphism Cyber Card
        color: "#151b24"
        border.color: root.hoverType === "process" ? root.colCyan : (root.hoverType === "battery" ? root.colGreen : root.colOrange)
        border.width: 1.5
        radius: 8

        // Drop shadow feel
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: 8
            color: "transparent"
            border.color: "#3300d2ff"
            border.width: 1
            opacity: 0.4
            z: -1
        }

        // Pinned Indicator & Close Button (REF-REQ-038)
        RowLayout {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 6
            spacing: 4
            z: 1000

            Rectangle {
                visible: root.hoverPinned
                width: 58; height: 16; radius: 3
                color: "#2a1e12"
                border.color: root.colOrange
                Text {
                    anchors.centerIn: parent
                    text: "📌 PINNED"
                    color: root.colOrange
                    font.pixelSize: 8
                    font.bold: true
                }
            }

            Rectangle {
                width: 16; height: 16; radius: 3
                color: closeHoverMa.containsMouse ? "#ef4444" : "#222a36"
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: "#ffffff"
                    font.pixelSize: 9
                    font.bold: true
                }
                MouseArea {
                    id: closeHoverMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.hoverPinned = false;
                        root.hoverVisible = false;
                    }
                }
            }
        }

        // =========================================================
        // PROCESS HOVER DETAILS (When hovering a process table row)
        // =========================================================
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            visible: root.hoverType === "process" && root.hoverData !== null

            // Header: Comm, PID, UID & Tier Badge
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    width: 24; height: 24; radius: 4
                    color: root.hoverData && root.hoverData["tier"] === 0 ? "#142921" : (root.hoverData && root.hoverData["tier"] === 5 ? "#361313" : "#1e2838")
                    border.color: root.hoverData && root.hoverData["tier"] === 0 ? root.colGreen : (root.hoverData && root.hoverData["tier"] === 5 ? root.colRed : root.colCyan)
                    Text {
                        anchors.centerIn: parent
                        text: root.hoverData ? ("T" + root.hoverData["tier"]) : "T?"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                ColumnLayout {
                    spacing: 1
                    Text {
                        text: root.hoverData ? (root.hoverData["comm"] + " (PID " + root.hoverData["pid"] + ", UID " + root.hoverData["uid"] + ")") : ""
                        color: root.textMain
                        font.bold: true
                        font.pixelSize: 12
                        font.family: "Monospace"
                    }
                    Text {
                        text: root.hoverData ? ("안전 등급: Tier " + root.hoverData["tier"] + " | WDI 피로 지수: " + (root.hoverData["wdiScore"] ? root.hoverData["wdiScore"].toFixed(1) : "0.0")) : ""
                        color: root.textDim
                        font.pixelSize: 9
                    }
                }

                Item { Layout.fillWidth: true }

                // Total Attributed Watts
                ColumnLayout {
                    spacing: 1
                    Text {
                        text: root.hoverData ? ((root.hoverData["totalWatts"] >= 1.0 ? root.hoverData["totalWatts"].toFixed(2) + " W" : (root.hoverData["totalWatts"] * 1000).toFixed(0) + " mW")) : ""
                        color: root.colOrange
                        font.bold: true
                        font.pixelSize: 14
                        font.family: "Monospace"
                        Layout.alignment: Qt.AlignRight
                    }
                    Text {
                        text: root.hoverData ? ("기여율: " + (root.hoverData["ratioPercent"] ? root.hoverData["ratioPercent"].toFixed(1) : "0") + "%") : ""
                        color: root.textMuted
                        font.pixelSize: 9
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            // Section 1: Detailed Physical Power Breakdown (Grid)
            Text { text: "⚡ 물리 하드웨어 도메인별 전력 분해 (Attributed Power):"; color: root.colCyan; font.bold: true; font.pixelSize: 9 }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 2
                columnSpacing: 6

                Text { text: "💻 CPU 연산: " + (root.hoverData ? (root.hoverData["cpuWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "🎮 GPU 실리콘: " + (root.hoverData ? (root.hoverData["gpuWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.colGreen; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "🧠 DRAM 버스: " + (root.hoverData ? (root.hoverData["dramWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.colPurple; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "⚡ 웨이크업 벌금: " + (root.hoverData ? (root.hoverData["wakeTaxWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.colOrange; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "💾 디스크 I/O: " + (root.hoverData ? (root.hoverData["ioWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "🌪️ 유도 팬 전력: " + (root.hoverData ? (root.hoverData["fanWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            // Section 2: CPU Execution & Scheduling Telemetry
            Text { text: "⚙️ 스케줄러 및 CPU 실행 프로필:"; color: root.colCyan; font.bold: true; font.pixelSize: 9 }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "고정 코어: Core #" + (root.hoverData && root.hoverData["cpuCore"] >= 0 ? root.hoverData["cpuCore"] : "All") + (root.hoverData && root.hoverData["crossCcx"] === 1 ? " (!CCX이동)" : "")
                    color: root.hoverData && root.hoverData["crossCcx"] === 1 ? root.colRed : root.textMain
                    font.pixelSize: 9
                }
                Item { Layout.fillWidth: true }
                Text { text: "스레드: " + (root.hoverData ? root.hoverData["threads"] : 1) + "개"; color: root.textDim; font.pixelSize: 9 }
                Item { Layout.fillWidth: true }
                Text { text: "Nice/Pri: " + (root.hoverData ? root.hoverData["nice"] : 0) + " / " + (root.hoverData ? root.hoverData["priority"] : 20); color: root.textDim; font.pixelSize: 9 }
                Item { Layout.fillWidth: true }
                Text { text: "웨이크업: " + (root.hoverData ? root.hoverData["wakeupsSec"] : 0) + "/s"; color: root.colOrange; font.pixelSize: 9 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            // Section 3: Memory, VFS & Peripherals
            Text { text: "💾 메모리 & VFS I/O 상태:"; color: root.colCyan; font.bold: true; font.pixelSize: 9 }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "정격 PSS: " + (root.hoverData ? root.hoverData["pssMb"] : 0) + " MB"; color: root.textMain; font.pixelSize: 9; font.family: "Monospace" }
                Item { Layout.fillWidth: true }
                Text { text: "VRAM: " + (root.hoverData ? (root.hoverData["vramMb"] ? root.hoverData["vramMb"].toFixed(0) : "0") : "0") + " MB"; color: root.colGreen; font.pixelSize: 9; font.family: "Monospace" }
                Item { Layout.fillWidth: true }
                Text { text: "네트워크: " + (root.hoverData ? root.hoverData["openSockets"] : 0) + " Sockets"; color: root.colBlue; font.pixelSize: 9 }
                Item { Layout.fillWidth: true }
                Text { text: "I/O: " + (root.hoverData ? (root.hoverData["ioMbSec"] ? root.hoverData["ioMbSec"].toFixed(2) : "0.0") : "0.0") + " MB/s"; color: root.textDim; font.pixelSize: 9 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            // Section 4: Physical Mechanism Diagnosis
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text { text: "🔍 WattCurb 원인 규명 및 진단 메커니즘:"; color: root.colOrange; font.bold: true; font.pixelSize: 9 }
                Text {
                    text: root.hoverData ? ("[" + root.hoverData["domain"] + "] " + root.hoverData["mechanism"]) : ""
                    color: root.textMain
                    font.pixelSize: 9
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // =========================================================
        // CPU HOVER DETAILS (When hovering CPU Card)
        // =========================================================
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6
            visible: root.hoverType === "cpu"

            RowLayout {
                Text { text: "💻 CPU RAPL & PMU 하드웨어 성능 카운터 심층 텔레메트리"; color: root.colCyan; font.bold: true; font.pixelSize: 11 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 4
                columnSpacing: 10

                Text { text: "CPU 주파수 거버너 (Governor): " + backend.cpuGovernor; color: root.textMain; font.pixelSize: 10 }
                Text { text: "PMU IPC (명령어/사이클): " + backend.pmuIpc.toFixed(2); color: root.colCyan; font.bold: true; font.pixelSize: 10; font.family: "Monospace" }
                Text { text: "총 실행 명령어 (Instructions): " + backend.pmuInstructions.toLocaleString(); color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "CPU 클럭 사이클 (Cycles): " + backend.pmuCycles.toLocaleString(); color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "LLC 캐시 미스 (LLC Misses): " + backend.pmuLlcMisses.toLocaleString(); color: root.colOrange; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "분기 예측 실패 (Branch Misses): " + backend.pmuBranchMisses.toLocaleString(); color: root.colOrange; font.pixelSize: 9; font.family: "Monospace" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Energy Waste Ratio (EWR): " + backend.pmuEwr.toFixed(1) + "%"; color: backend.pmuEwr > 20 ? root.colRed : root.colGreen; font.bold: true; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Text { text: "Zero-Wakeup Timer Coalescing Active"; color: root.textMuted; font.pixelSize: 9 }
            }
            Text {
                text: "* EWR: 캐시 미스 및 파이프라인 스톨로 인해 낭비된 CPU 에너지 백분율 (낮을수록 에너지 효율적)"
                color: root.textMuted
                font.pixelSize: 8
            }
        }

        // =========================================================
        // BATTERY HOVER DETAILS (When hovering Battery Card)
        // =========================================================
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6
            visible: root.hoverType === "battery"

            RowLayout {
                Text { text: "🔋 BAT0 배터리 화학 및 전기적 물리 사양 심층 텔레메트리"; color: root.colGreen; font.bold: true; font.pixelSize: 11 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 4
                columnSpacing: 10

                Text { text: "배터리 셀 제조사: " + backend.batteryMfg; color: root.textMain; font.pixelSize: 10 }
                Text { text: "배터리 모델명: " + backend.batteryModel; color: root.textMain; font.pixelSize: 10 }
                Text { text: "배터리 화학 기술: " + backend.batteryTech + " (SMP)"; color: root.textDim; font.pixelSize: 9 }
                Text { text: "충방전 완충 사이클: " + backend.batteryCycles + " 회"; color: root.textDim; font.pixelSize: 9 }
                Text { text: "정격 설계 용량 (Design): " + backend.batteryDesignWh.toFixed(2) + " Wh"; color: root.textDim; font.pixelSize: 9; font.family: "Monospace" }
                Text { text: "현재 만충 용량 (Full): " + backend.batteryFullWh.toFixed(2) + " Wh"; color: root.colGreen; font.bold: true; font.pixelSize: 9; font.family: "Monospace" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "배터리 건강도 보존율: " + backend.batteryHealth + "% (열화율 " + (100 - backend.batteryHealth) + "%)"; color: root.colGreen; font.bold: true; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Text { text: backend.batteryState === 2 ? "AC Pass-through: ON" : "Normal Discharge"; color: root.colCyan; font.pixelSize: 9; font.bold: true }
            }
            Text {
                text: "* AC Pass-through: 배터리 충전을 우회하고 시스템에 직접 전력을 공급하여 사이클 마모율 0% 유지"
                color: root.textMuted
                font.pixelSize: 8
            }
        }
    }
}
