import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 840
    minimumWidth: 1080
    minimumHeight: 720
    title: "WattCurb btop-Style Power & Hardware Matrix Dashboard"
    color: "#0b0e12"

    // btop cyber terminal palette
    readonly property color bgApp: "#0b0e12"
    readonly property color bgPanel: "#12161d"
    readonly property color bgPanelHeader: "#181e26"
    readonly property color borderPanel: "#222a36"
    readonly property color bgRowAlt: "#151922"
    readonly property color textMain: "#e5e7eb"
    readonly property color textDim: "#9ca3af"
    readonly property color textMuted: "#6b7280"
    
    readonly property color colCyan: "#00d2ff"
    readonly property color colGreen: "#10b981"
    readonly property color colOrange: "#f59e0b"
    readonly property color colRed: "#ef4444"
    readonly property color colPurple: "#a855f7"
    readonly property color colBlue: "#3b82f6"

    // Responsive Hover Inspection State (REF-REQ-038)
    property bool hoverVisible: false
    property string hoverType: "" // "process", "cpu", "battery", "gpu"
    property var hoverData: null
    property real hoverTargetX: 0
    property real hoverTargetY: 0

    // Standalone Deep Battery Drain Report Window (REF-REQ-078, REF-ARCH-055)
    BatteryReportWindow {
        id: reportWindow
    }

    // =========================================================================
    // REUSABLE TACTILE CYBER BUTTON COMPONENT (REF-REQ-081, REF-ARCH-058)
    // Physical depression (scale: 0.95), distinct hover lighting, and border glow
    // =========================================================================
    component TactileButton: Button {
        id: tBtn
        property color accentColor: root.colCyan
        property color baseColor: "#1a222e"
        property color hoverColor: "#263345"
        property color pressColor: "#0e1520"
        property color textColor: "#e2e8f0"
        property real customRadius: 6

        implicitHeight: 30
        hoverEnabled: true

        scale: !enabled ? 1.0 : (down ? 0.95 : (hovered ? 1.03 : 1.0))
        Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }

        contentItem: RowLayout {
            spacing: 5
            anchors.centerIn: parent
            Text {
                text: tBtn.text
                font.pixelSize: 11
                font.bold: true
                color: !tBtn.enabled ? "#64748b" : (tBtn.highlighted ? "#ffffff" : (tBtn.hovered ? "#ffffff" : tBtn.textColor))
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                y: tBtn.down ? 1 : 0
            }
        }

        background: Rectangle {
            radius: tBtn.customRadius
            color: !tBtn.enabled ? "#11161f" : (
                tBtn.down ? tBtn.pressColor : (
                    tBtn.highlighted ? Qt.rgba(tBtn.accentColor.r, tBtn.accentColor.g, tBtn.accentColor.b, 0.32) : (
                        tBtn.hovered ? tBtn.hoverColor : tBtn.baseColor
                    )
                )
            )
            border.color: !tBtn.enabled ? "#243042" : (
                tBtn.highlighted ? tBtn.accentColor : (
                    tBtn.hovered ? tBtn.accentColor : "#37475d"
                )
            )
            border.width: (tBtn.highlighted || tBtn.hovered) ? 1.5 : 1

            // Subtle top bevel highlight for tactile 3D realism
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 1
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                height: 1
                color: tBtn.down ? "transparent" : (tBtn.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(1, 1, 1, 0.08))
                radius: tBtn.customRadius
            }

            // Subtle bottom shadow line
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 1
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                height: 1
                color: tBtn.down ? "transparent" : Qt.rgba(0, 0, 0, 0.4)
                radius: tBtn.customRadius
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: tBtn.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            acceptedButtons: Qt.NoButton
        }
    }

    Connections {
        target: backend
        function onReportWindowRequested() {
            reportWindow.visible = true;
            reportWindow.show();
            reportWindow.raise();
            reportWindow.requestActivate();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // =============================================================
        // 1. TOP DENSE HEADER BAR (46px)
        // =============================================================
        Rectangle {
            Layout.fillWidth: true
            height: 46
            color: root.bgPanel
            border.color: root.borderPanel
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // App Title & Brand
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 26; height: 26; radius: 4; color: "#0284c7"
                        Text { anchors.centerIn: parent; text: "⚡"; font.pixelSize: 15 }
                    }
                    Text {
                        text: "WATTCURB"
                        color: root.colCyan
                        font.bold: true
                        font.pixelSize: 16
                        font.family: "Monospace"
                    }
                    Text {
                        text: "[btop Deep Hardware & Power Matrix]"
                        color: root.textDim
                        font.pixelSize: 12
                    }
                }

                Item { Layout.fillWidth: true }

                // Big Total System Drain Display
                RowLayout {
                    spacing: 8
                    Text {
                        text: backend.tr("TOTAL_DRAIN") + ":"
                        color: root.textDim
                        font.bold: true
                        font.pixelSize: 12
                    }
                    Text {
                        text: backend.systemDrainWatts.toFixed(2) + " W"
                        color: backend.systemDrainWatts > 25.0 ? root.colRed : (backend.systemDrainWatts > 15.0 ? root.colOrange : root.colCyan)
                        font.bold: true
                        font.pixelSize: 22
                        font.family: "Monospace"
                    }
                }

                // Battery State Indicator
                Rectangle {
                    height: 28
                    Layout.preferredWidth: batStatusText.implicitWidth + 20
                    radius: 4
                    color: backend.batteryState === 1 ? "#361c0a" : "#0d2b1d"
                    border.color: backend.batteryState === 1 ? root.colOrange : root.colGreen
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: backend.batteryState === 1 ? root.colOrange : root.colGreen
                        }
                        Text {
                            id: batStatusText
                            text: backend.batteryPercent + "% (" + backend.batteryStateString + ")"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                }

                // Profile Badge
                Rectangle {
                    height: 28
                    Layout.preferredWidth: profBadgeText.implicitWidth + 18
                    radius: 4
                    color: "#1e293b"
                    border.color: root.colCyan
                    border.width: 1
                    Text {
                        id: profBadgeText
                        anchors.centerIn: parent
                        text: backend.powerProfileName
                        color: root.colCyan
                        font.bold: true
                        font.pixelSize: 12
                    }
                }

                // Deep Battery Report Standalone Window Button (REF-REQ-078, REF-REQ-081)
                TactileButton {
                    text: "🔋 배터리 정밀 분석 리포트"
                    accentColor: root.colGreen
                    baseColor: "#0d2e24"
                    hoverColor: "#134234"
                    pressColor: "#081a14"
                    textColor: "#34d399"
                    onClicked: {
                        backend.generateBatteryReport();
                        reportWindow.visible = true;
                        reportWindow.show();
                        reportWindow.raise();
                        reportWindow.requestActivate();
                    }
                }

                // Last Sync Time
                Text {
                    text: "SYNC " + backend.lastUpdateTime
                    color: root.textMuted
                    font.pixelSize: 11
                    font.family: "Monospace"
                }
            }
        }

        // =============================================================
        // 2. MAIN WORKSPACE (LEFT: HARDWARE DOMAINS, RIGHT: TIMELINE & PROCESSES)
        // =============================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // ---------------------------------------------------------
            // LEFT COLUMN: DENSE HARDWARE METRICS WITH SPARKLINE GRAPHS (460px)
            // ---------------------------------------------------------
            ColumnLayout {
                Layout.preferredWidth: 460
                Layout.minimumWidth: 460
                Layout.maximumWidth: 460
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: 8

                // CARD 1: CPU & Memory Subsystem (RAPL) + Real-Time Sparkline
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 210
                    color: cpuMa.containsMouse ? "#161b24" : root.bgPanel
                    border.color: cpuMa.containsMouse ? root.colCyan : root.borderPanel
                    radius: 6
                    clip: true

                    MouseArea {
                        id: cpuMa
                        anchors.fill: parent
                        hoverEnabled: true
                        z: 10
                        onEntered: {
                            var pos = mapToItem(root.contentItem, mouseX, mouseY);
                            root.hoverTargetX = pos.x;
                            root.hoverTargetY = pos.y;
                            root.hoverType = "cpu";
                            root.hoverVisible = true;
                        }
                        onPositionChanged: {
                            var pos = mapToItem(root.contentItem, mouseX, mouseY);
                            root.hoverTargetX = pos.x;
                            root.hoverTargetY = pos.y;
                        }
                        onExited: {
                            root.hoverVisible = false;
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        // Title & Live Watts
                        RowLayout {
                            Text { text: "💻 " + backend.tr("CPU_MEM"); color: root.colCyan; font.bold: true; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true }
                            Text { text: "🔍 " + backend.tr("DETAILS"); color: root.textMuted; font.pixelSize: 11 }
                            Text { text: backend.cpuDrainWatts.toFixed(2) + " W"; color: root.colCyan; font.bold: true; font.pixelSize: 18; font.family: "Monospace" }
                        }

                        // Detailed Breakdowns (Pkg, Core, Uncore, DRAM)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: "Pkg: " + backend.cpuDrainWatts.toFixed(2) + "W"; color: root.textMain; font.pixelSize: 12; font.family: "Monospace" }
                            Text { text: "| Core: " + backend.cpuCoreWatts.toFixed(2) + "W"; color: root.textDim; font.pixelSize: 12; font.family: "Monospace" }
                            Text { text: "| Uncore: " + backend.cpuUncoreWatts.toFixed(2) + "W"; color: root.textDim; font.pixelSize: 12; font.family: "Monospace" }
                            Text { text: "| DRAM: " + backend.cpuDramWatts.toFixed(2) + "W"; color: root.colPurple; font.pixelSize: 12; font.family: "Monospace" }
                        }

                        // Temp, Frequency, Fan
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text {
                                text: "Core Temp: " + backend.cpuTempC + "°C"
                                color: backend.cpuTempC > 75 ? root.colRed : (backend.cpuTempC > 60 ? root.colOrange : root.colGreen)
                                font.pixelSize: 12; font.bold: true
                            }
                            Text { text: "Freq: " + backend.cpuFreqMhz + " MHz (" + backend.cpuGovernor + ")"; color: root.textMain; font.pixelSize: 12; font.family: "Monospace" }
                            Text { text: "Fan: " + backend.fanRpm + " RPM"; color: root.textDim; font.pixelSize: 12 }
                        }

                        // [GRAPH] Real-Time CPU Power Sparkline Graph
                        Canvas {
                            id: cpuSparkCanvas
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            antialiasing: true
                            renderStrategy: Canvas.Threaded
                            renderTarget: Canvas.FramebufferObject

                            Connections {
                                target: backend
                                function onHistoryChanged() { cpuSparkCanvas.requestPaint(); }
                            }

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                if (w <= 0 || h <= 0) return;
                                var data = backend.cpuHistory;
                                if (!data || data.length < 2) return;

                                var maxVal = 20.0;
                                for (var i = 0; i < data.length; ++i) {
                                    if (data[i] > maxVal) maxVal = data[i];
                                }

                                var step = w / (data.length - 1);
                                ctx.beginPath();
                                ctx.moveTo(0, h * (1.0 - Math.min(1.0, data[0] / maxVal)));
                                for (var j = 1; j < data.length; ++j) {
                                    ctx.lineTo(j * step, h * (1.0 - Math.min(1.0, data[j] / maxVal)));
                                }
                                ctx.strokeStyle = "#38bdf8";
                                ctx.lineWidth = 1.5;
                                ctx.stroke();

                                ctx.lineTo(w, h);
                                ctx.lineTo(0, h);
                                ctx.closePath();
                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0.0, "rgba(56, 189, 248, 0.3)");
                                grad.addColorStop(1.0, "rgba(56, 189, 248, 0.0)");
                                ctx.fillStyle = grad;
                                ctx.fill();
                            }
                        }

                        // C-State Sleep Residencies
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 2
                                RowLayout {
                                    Text { text: "C0 (Active)"; color: root.textDim; font.pixelSize: 11 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC0Percent.toFixed(1) + "%"; color: root.textMain; font.pixelSize: 11; font.family: "Monospace" }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 6; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC0Percent / 100.0); height: parent.height; radius: 2; color: root.colOrange }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 2
                                RowLayout {
                                    Text { text: "C1"; color: root.textDim; font.pixelSize: 11 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC1Percent.toFixed(1) + "%"; color: root.textMain; font.pixelSize: 11; font.family: "Monospace" }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 6; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC1Percent / 100.0); height: parent.height; radius: 2; color: root.colBlue }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 2
                                RowLayout {
                                    Text { text: "C2"; color: root.textDim; font.pixelSize: 11 }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC2Percent.toFixed(1) + "%"; color: root.textMain; font.pixelSize: 11; font.family: "Monospace" }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 6; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC2Percent / 100.0); height: parent.height; radius: 2; color: root.colBlue }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true; spacing: 2
                                RowLayout {
                                    Text { text: "C3 (Deep Sleep)"; color: root.colGreen; font.pixelSize: 11; font.bold: true }
                                    Item { Layout.fillWidth: true }
                                    Text { text: backend.cstateC3Percent.toFixed(1) + "%"; color: root.colGreen; font.pixelSize: 11; font.family: "Monospace"; font.bold: true }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; height: 6; radius: 2; color: "#222a36"
                                    Rectangle { width: parent.width * Math.min(1.0, backend.cstateC3Percent / 100.0); height: parent.height; radius: 2; color: root.colGreen }
                                }
                            }
                        }

                        // PMU IPC & Waste Ratio
                        Text {
                            text: "PMU IPC: " + backend.pmuIpc.toFixed(2) + " | Waste Ratio: " + backend.pmuEwr.toFixed(1) + "% | " + backend.wakeupsPerSec + " wakeups/s"
                            color: root.textMuted
                            font.pixelSize: 11
                            font.family: "Monospace"
                            Layout.fillWidth: true
                        }
                    }
                }

                // CARD 2: Battery & Electrical Telemetry (BAT0) + Discharge Sparkline
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    color: batMa.containsMouse ? "#161b24" : root.bgPanel
                    border.color: batMa.containsMouse ? root.colGreen : root.borderPanel
                    radius: 6
                    clip: true

                    MouseArea {
                        id: batMa
                        anchors.fill: parent
                        hoverEnabled: true
                        z: 10
                        onEntered: {
                            var pos = mapToItem(root.contentItem, mouseX, mouseY);
                            root.hoverTargetX = pos.x;
                            root.hoverTargetY = pos.y;
                            root.hoverType = "battery";
                            root.hoverVisible = true;
                        }
                        onPositionChanged: {
                            var pos = mapToItem(root.contentItem, mouseX, mouseY);
                            root.hoverTargetX = pos.x;
                            root.hoverTargetY = pos.y;
                        }
                        onExited: {
                            root.hoverVisible = false;
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        RowLayout {
                            Text { text: "🔋 " + backend.tr("BATTERY") + " (BAT0)"; color: root.colGreen; font.bold: true; font.pixelSize: 13 }
                            Item { Layout.fillWidth: true }

                            Rectangle {
                                height: 20
                                Layout.preferredWidth: batReportBtnText.implicitWidth + 14
                                radius: 3
                                color: batReportMa.containsMouse ? "#059669" : "#0d2b1d"
                                border.color: root.colGreen
                                border.width: 1
                                Text {
                                    id: batReportBtnText
                                    anchors.centerIn: parent
                                    text: "⚡ 전수 리포트"
                                    color: "#ffffff"
                                    font.bold: true
                                    font.pixelSize: 10
                                }
                                MouseArea {
                                    id: batReportMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        backend.generateBatteryReport();
                                        reportWindow.visible = true;
                                        reportWindow.show();
                                        reportWindow.raise();
                                        reportWindow.requestActivate();
                                    }
                                }
                            }

                            Text { text: backend.batteryPercent + "%"; color: root.colGreen; font.bold: true; font.pixelSize: 20; font.family: "Monospace" }
                        }

                        // Big Battery Level Bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 7; radius: 3; color: "#222a36"
                            Rectangle {
                                width: parent.width * Math.min(1.0, backend.batteryPercent / 100.0)
                                height: parent.height; radius: 3
                                color: backend.batteryPercent < 20 ? root.colRed : (backend.batteryPercent < 45 ? root.colOrange : root.colGreen)
                            }
                        }

                        // Voltage, Current, Cycles, Health, Time
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: "Volt: " + backend.batteryVoltageV.toFixed(2) + " V"; color: root.textMain; font.pixelSize: 12; font.family: "Monospace" }
                            Text { text: "Curr: " + backend.batteryCurrentA.toFixed(2) + " A"; color: root.textMain; font.pixelSize: 12; font.family: "Monospace" }
                            Item { Layout.fillWidth: true }
                            Text { text: "Cycles: " + backend.batteryCycles; color: root.textDim; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: "Health: " + backend.batteryHealth + "% (" + backend.batteryTech + ")"; color: root.colGreen; font.pixelSize: 12; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: "Time: " + backend.timeToEmptyString; color: root.textMain; font.pixelSize: 12; font.bold: true }
                        }

                        // [GRAPH] Battery Trend Sparkline Graph
                        Canvas {
                            id: batSparkCanvas
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            antialiasing: true
                            renderStrategy: Canvas.Threaded
                            renderTarget: Canvas.FramebufferObject

                            Connections {
                                target: backend
                                function onHistoryChanged() { batSparkCanvas.requestPaint(); }
                            }

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                if (w <= 0 || h <= 0) return;
                                var data = backend.systemHistory;
                                if (!data || data.length < 2) return;

                                var maxVal = Math.max(25.0, backend.peakSystemWatts);
                                var step = w / (data.length - 1);
                                ctx.beginPath();
                                ctx.moveTo(0, h * (1.0 - Math.min(1.0, data[0] / maxVal)));
                                for (var j = 1; j < data.length; ++j) {
                                    ctx.lineTo(j * step, h * (1.0 - Math.min(1.0, data[j] / maxVal)));
                                }
                                ctx.strokeStyle = "#10b981";
                                ctx.lineWidth = 1.5;
                                ctx.stroke();

                                ctx.lineTo(w, h);
                                ctx.lineTo(0, h);
                                ctx.closePath();
                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0.0, "rgba(16, 185, 129, 0.25)");
                                grad.addColorStop(1.0, "rgba(16, 185, 129, 0.0)");
                                ctx.fillStyle = grad;
                                ctx.fill();
                            }
                        }

                        Text {
                            text: backend.batteryState === 2 ? "AC Hardware Pass-through Active (Zero wear)" : "Adaptive Power Optimization Active (" + backend.batteryModel + ")"
                            color: root.textMuted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                // CARD 3: GPU, Display & Storage Subsystem (Fills Remainder)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.bgPanel
                    border.color: root.borderPanel
                    radius: 6
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 5

                        // GPU Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "🎮 " + backend.tr("GPU") + ":"; color: root.textMain; font.pixelSize: 13; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.gpuDrainWatts.toFixed(2) + " W"; color: root.colGreen; font.pixelSize: 16; font.bold: true; font.family: "Monospace" }
                            Text { text: "(Load: " + backend.gpuLoadPercent + "%)"; color: root.colGreen; font.bold: true; font.pixelSize: 12 }
                        }
                        Text { text: "AMD Radeon 780M / Dynamic Power-Gating"; color: root.textMuted; font.pixelSize: 11 }

                        // [GRAPH] Real-Time GPU Load Sparkline
                        Canvas {
                            id: gpuSparkCanvas
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            antialiasing: true
                            renderStrategy: Canvas.Threaded
                            renderTarget: Canvas.FramebufferObject

                            Connections {
                                target: backend
                                function onHistoryChanged() { gpuSparkCanvas.requestPaint(); }
                            }

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                if (w <= 0 || h <= 0) return;
                                var data = backend.gpuHistory;
                                if (!data || data.length < 2) return;

                                var maxVal = 15.0;
                                for (var i = 0; i < data.length; ++i) {
                                    if (data[i] > maxVal) maxVal = data[i];
                                }

                                var step = w / (data.length - 1);
                                ctx.beginPath();
                                ctx.moveTo(0, h * (1.0 - Math.min(1.0, data[0] / maxVal)));
                                for (var j = 1; j < data.length; ++j) {
                                    ctx.lineTo(j * step, h * (1.0 - Math.min(1.0, data[j] / maxVal)));
                                }
                                ctx.strokeStyle = "#a855f7";
                                ctx.lineWidth = 1.5;
                                ctx.stroke();

                                ctx.lineTo(w, h);
                                ctx.lineTo(0, h);
                                ctx.closePath();
                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0.0, "rgba(168, 85, 247, 0.25)");
                                grad.addColorStop(1.0, "rgba(168, 85, 247, 0.0)");
                                ctx.fillStyle = grad;
                                ctx.fill();
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#19202a" }

                        // Display
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "🖥️ " + backend.tr("DISPLAY") + ":"; color: root.textMain; font.pixelSize: 13; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.displayDrainWatts.toFixed(2) + " W"; color: root.colOrange; font.pixelSize: 14; font.bold: true; font.family: "Monospace" }
                            Text { text: "(" + backend.displayBrightnessPct + "% bright)"; color: root.textDim; font.pixelSize: 12 }
                        }
                        Text { text: "amdgpu_bl1 Adaptive Dynamic Dimming & VRR Ready"; color: root.textMuted; font.pixelSize: 11 }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#19202a" }

                        // Storage
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "💾 " + backend.tr("STORAGE") + ":"; color: root.textMain; font.pixelSize: 13; font.bold: true }
                            Item { Layout.fillWidth: true }
                            Text { text: backend.nvmeDrainWatts.toFixed(2) + " W"; color: root.colCyan; font.pixelSize: 14; font.bold: true; font.family: "Monospace" }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "APST L1.2 Ultra-Low Sleep (" + backend.aspmPolicy + ")"; color: root.colGreen; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: "R: " + backend.diskReadMbPerSec.toFixed(1) + "M | W: " + backend.diskWriteMbPerSec.toFixed(1) + "MB/s"; color: root.textDim; font.pixelSize: 11; font.family: "Monospace" }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // RIGHT COLUMN: btop TIMELINE GRAPH + PROCESS ATTRIBUTION MATRIX (Fills Remainder)
            // ---------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                // TOP GRAPH: System Total Power Consumption Timeline (85px)
                Rectangle {
                    Layout.fillWidth: true
                    height: 85
                    color: root.bgPanel
                    border.color: root.borderPanel
                    radius: 6
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "📈 " + backend.tr("TIMELINE") + " (35s)"
                                color: root.colCyan
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "Live: " + backend.systemDrainWatts.toFixed(2) + " W | Peak: " + backend.peakSystemWatts.toFixed(2) + " W"
                                color: root.colOrange
                                font.bold: true
                                font.pixelSize: 12
                                font.family: "Monospace"
                            }
                        }

                        // Canvas Waveform with Grid Lines & Gradient
                        Canvas {
                            id: sysTimelineCanvas
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            antialiasing: true
                            renderStrategy: Canvas.Threaded
                            renderTarget: Canvas.FramebufferObject

                            Connections {
                                target: backend
                                function onHistoryChanged() { sysTimelineCanvas.requestPaint(); }
                            }

                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                if (w <= 0 || h <= 0) return;

                                var data = backend.systemHistory;
                                if (!data || data.length < 2) return;

                                var maxVal = Math.max(25.0, backend.peakSystemWatts * 1.05);

                                // Background Horizontal Grid Lines
                                ctx.strokeStyle = "#1a2433";
                                ctx.lineWidth = 1.0;
                                ctx.beginPath();
                                for (var g = 1; g <= 3; ++g) {
                                    var gy = h * (1.0 - g / 4.0);
                                    ctx.moveTo(0, gy);
                                    ctx.lineTo(w, gy);
                                }
                                ctx.stroke();

                                // Waveform Path
                                var step = w / (data.length - 1);
                                ctx.beginPath();
                                ctx.moveTo(0, h * (1.0 - Math.min(1.0, data[0] / maxVal)));
                                for (var i = 1; i < data.length; ++i) {
                                    ctx.lineTo(i * step, h * (1.0 - Math.min(1.0, data[i] / maxVal)));
                                }

                                ctx.strokeStyle = "#00d2ff";
                                ctx.lineWidth = 2.0;
                                ctx.stroke();

                                // Fill Area with Cyan Gradient
                                ctx.lineTo(w, h);
                                ctx.lineTo(0, h);
                                ctx.closePath();
                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0.0, "rgba(0, 210, 255, 0.35)");
                                grad.addColorStop(1.0, "rgba(0, 210, 255, 0.0)");
                                ctx.fillStyle = grad;
                                ctx.fill();
                            }
                        }
                    }
                }

                // BOTTOM: btop Process Power Matrix Table (Fills Remainder)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: root.bgPanel
                    border.color: root.borderPanel
                    radius: 6
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        // Table Header Bar
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: "⚡ TOP PROCESS ATTRIBUTION MATRIX (btop Power Profiler)"
                                color: root.colCyan
                                font.bold: true
                                font.pixelSize: 13
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "💡 마우스를 올리면 전력/스케줄러/메모리 실시간 심층 카드 표시"
                                color: root.colOrange
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // Table Column Headers
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: root.bgPanelHeader
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 6

                                Text { text: "PID"; color: root.textDim; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 50; clip: true }
                                Text { text: "PROGRAM"; color: root.textDim; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 130; clip: true }
                                Text { text: "TOTAL"; color: root.textDim; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 70; clip: true }
                                Text { text: "RATIO"; color: root.textDim; font.pixelSize: 12; font.bold: true; Layout.preferredWidth: 78; clip: true }
                                Text { text: "CPU/GPU/DRAM"; color: root.textDim; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 115; clip: true }
                                Text { text: "PSS"; color: root.textDim; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 52; clip: true }
                                Text { text: "TIER"; color: root.textDim; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 55; clip: true }
                                Text { text: "PRIMARY HARDWARE MECHANISM"; color: root.textDim; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; clip: true }
                            }
                        }

                        // Process ListView
                        ListView {
                            id: procListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            reuseItems: true
                            cacheBuffer: 150
                            model: backend.processList
                            spacing: 3

                            delegate: Rectangle {
                                id: rowDelegate
                                width: procListView.width
                                height: 30
                                radius: 4
                                color: rowMa.containsMouse ? "#222f42" : (index % 2 === 0 ? root.bgRowAlt : root.bgPanel)
                                border.color: rowMa.containsMouse ? root.colCyan : "transparent"
                                border.width: 1

                                MouseArea {
                                    id: rowMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    z: 10
                                    onEntered: {
                                        var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                        root.hoverTargetX = pos.x;
                                        root.hoverTargetY = pos.y;
                                        root.hoverType = "process";
                                        root.hoverData = modelData;
                                        root.hoverVisible = true;
                                    }
                                    onPositionChanged: {
                                        var pos = mapToItem(root.contentItem, mouseX, mouseY);
                                        root.hoverTargetX = pos.x;
                                        root.hoverTargetY = pos.y;
                                    }
                                    onExited: {
                                        root.hoverVisible = false;
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 6

                                    // PID
                                    Text {
                                        text: modelData["pid"] !== undefined ? modelData["pid"] : ""
                                        color: root.textDim
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 50
                                        elide: Text.ElideRight
                                        clip: true
                                    }

                                    // Comm / Program Name
                                    Text {
                                        text: modelData["comm"] !== undefined ? modelData["comm"] : ""
                                        color: root.textMain
                                        font.bold: true
                                        font.pixelSize: 12
                                        Layout.preferredWidth: 130
                                        elide: Text.ElideRight
                                        clip: true
                                    }

                                    // Total Watts / mW
                                    Text {
                                        readonly property real w: modelData["totalWatts"] !== undefined ? modelData["totalWatts"] : 0.0
                                        text: w >= 1.0 ? (w.toFixed(2) + "W") : ((w * 1000).toFixed(0) + "m")
                                        color: w > 1.5 ? root.colRed : (w > 0.5 ? root.colOrange : root.colCyan)
                                        font.bold: true
                                        font.pixelSize: 12
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 70
                                        elide: Text.ElideRight
                                        clip: true
                                    }

                                    // Ratio Mini Bar
                                    RowLayout {
                                        Layout.preferredWidth: 78
                                        spacing: 4
                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 6; radius: 3; color: "#222a36"
                                            Rectangle {
                                                readonly property real r: modelData["ratioPercent"] !== undefined ? modelData["ratioPercent"] : 0.0
                                                width: parent.width * Math.min(1.0, r / 100.0)
                                                height: parent.height; radius: 3
                                                color: r > 10.0 ? root.colOrange : root.colCyan
                                            }
                                        }
                                        Text {
                                            readonly property real r: modelData["ratioPercent"] !== undefined ? modelData["ratioPercent"] : 0.0
                                            text: r.toFixed(0) + "%"
                                            color: root.textDim
                                            font.pixelSize: 11
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
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 115
                                        elide: Text.ElideRight
                                        clip: true
                                    }

                                    // PSS (Memory)
                                    Text {
                                        text: (modelData["pssMb"] !== undefined ? modelData["pssMb"] : 0) + "M"
                                        color: root.textMain
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 52
                                        elide: Text.ElideRight
                                        clip: true
                                    }

                                    // Safety Tier Badge
                                    Rectangle {
                                        Layout.preferredWidth: 55
                                        height: 20
                                        radius: 3
                                        readonly property int t: modelData["tier"] !== undefined ? modelData["tier"] : 0
                                        color: t === 0 ? "#142921" : (t === 5 ? "#361313" : "#1b222d")
                                        border.color: t === 0 ? root.colGreen : (t === 5 ? root.colRed : root.borderPanel)
                                        Text {
                                            anchors.centerIn: parent
                                            text: "Tier " + parent.t
                                            color: parent.t === 0 ? root.colGreen : (parent.t === 5 ? root.colRed : root.textDim)
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    // Primary Hardware Mechanism
                                    Text {
                                        text: (modelData["domain"] !== undefined ? ("[" + modelData["domain"] + "] ") : "") + (modelData["mechanism"] !== undefined ? modelData["mechanism"] : "")
                                        color: root.textDim
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                        clip: true
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =============================================================
        // 3. LOWER POWER SHARE ANALYTICS DECK (REF-REQ-061, REF-ARCH-037)
        // =============================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 155
            spacing: 8

            // CARD A: Hardware Devices Power Share Donut (Aligned with Left Column 460px)
            Rectangle {
                Layout.preferredWidth: 460
                Layout.minimumWidth: 460
                Layout.maximumWidth: 460
                Layout.fillHeight: true
                color: root.bgPanel
                border.color: root.borderPanel
                radius: 6
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    // Header
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "💻 " + backend.tr("DASH_POWER_SHARE_HW")
                            color: root.colCyan
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: backend.totalDeviceWatts.toFixed(2) + " W"
                            color: root.colCyan
                            font.bold: true
                            font.pixelSize: 14
                            font.family: "Monospace"
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#19202a" }

                    // Donut + Legend
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 10

                        // Donut Canvas
                        Item {
                            width: 105; height: 105
                            Canvas {
                                id: devDonutCanvas
                                anchors.fill: parent
                                antialiasing: true
                                renderStrategy: Canvas.Threaded
                                renderTarget: Canvas.FramebufferObject

                                Connections {
                                    target: backend
                                    function onPowerSharesChanged() { devDonutCanvas.requestPaint(); }
                                }

                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    var w = width, h = height;
                                    if (w <= 0 || h <= 0) return;
                                    var cx = w / 2, cy = h / 2;
                                    var rOut = Math.min(w, h) / 2 - 3;
                                    var rIn = rOut * 0.64;

                                    var data = backend.devicePowerShares;
                                    if (!data || data.length === 0) {
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, rOut, 0, 2 * Math.PI);
                                        ctx.arc(cx, cy, rIn, 2 * Math.PI, 0, true);
                                        ctx.fillStyle = "#1e293b";
                                        ctx.fill();
                                        return;
                                    }

                                    var start = -Math.PI / 2;
                                    for (var i = 0; i < data.length; ++i) {
                                        var it = data[i];
                                        var sweep = (it.pct / 100.0) * (2 * Math.PI);
                                        if (sweep <= 0.001) continue;
                                        var end = start + sweep;

                                        ctx.beginPath();
                                        ctx.arc(cx, cy, rOut, start, end);
                                        ctx.arc(cx, cy, rIn, end, start, true);
                                        ctx.closePath();
                                        ctx.fillStyle = it.color;
                                        ctx.fill();

                                        ctx.lineWidth = 1.5;
                                        ctx.strokeStyle = root.bgPanel;
                                        ctx.stroke();

                                        start = end;
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 0
                                Text {
                                    text: backend.totalDeviceWatts.toFixed(1) + "W"
                                    color: root.textMain
                                    font.bold: true
                                    font.pixelSize: 14
                                    font.family: "Monospace"
                                    Layout.alignment: Qt.AlignCenter
                                }
                                Text {
                                    text: "DEVICE"
                                    color: root.textMuted
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.alignment: Qt.AlignCenter
                                }
                            }
                        }

                        // Legend Column
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2

                            Repeater {
                                model: backend.devicePowerShares
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Rectangle { width: 7; height: 7; radius: 2; color: modelData.color }
                                    Text {
                                        text: modelData.name
                                        color: root.textMain
                                        font.pixelSize: 11
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.watts.toFixed(1) + "W"
                                        color: modelData.color
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                    }
                                    Text {
                                        text: modelData.pct.toFixed(0) + "%"
                                        color: root.textDim
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 30
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // CARD B: Processes Power Share Donut (Aligned with Right Column fillWidth)
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: root.bgPanel
                border.color: root.borderPanel
                radius: 6
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    // Header
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "🚀 " + backend.tr("DASH_POWER_SHARE_PROC")
                            color: root.colOrange
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: backend.totalProcessWatts.toFixed(2) + " W"
                            color: root.colOrange
                            font.bold: true
                            font.pixelSize: 14
                            font.family: "Monospace"
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#19202a" }

                    // Donut + 2-Column Legend Grid
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12

                        // Donut Canvas
                        Item {
                            width: 105; height: 105
                            Canvas {
                                id: procDonutCanvas
                                anchors.fill: parent
                                antialiasing: true
                                renderStrategy: Canvas.Threaded
                                renderTarget: Canvas.FramebufferObject

                                Connections {
                                    target: backend
                                    function onPowerSharesChanged() { procDonutCanvas.requestPaint(); }
                                }

                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.reset();
                                    var w = width, h = height;
                                    if (w <= 0 || h <= 0) return;
                                    var cx = w / 2, cy = h / 2;
                                    var rOut = Math.min(w, h) / 2 - 3;
                                    var rIn = rOut * 0.64;

                                    var data = backend.processPowerShares;
                                    if (!data || data.length === 0) {
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, rOut, 0, 2 * Math.PI);
                                        ctx.arc(cx, cy, rIn, 2 * Math.PI, 0, true);
                                        ctx.fillStyle = "#1e293b";
                                        ctx.fill();
                                        return;
                                    }

                                    var start = -Math.PI / 2;
                                    for (var i = 0; i < data.length; ++i) {
                                        var it = data[i];
                                        var sweep = (it.pct / 100.0) * (2 * Math.PI);
                                        if (sweep <= 0.001) continue;
                                        var end = start + sweep;

                                        ctx.beginPath();
                                        ctx.arc(cx, cy, rOut, start, end);
                                        ctx.arc(cx, cy, rIn, end, start, true);
                                        ctx.closePath();
                                        ctx.fillStyle = it.color;
                                        ctx.fill();

                                        ctx.lineWidth = 1.5;
                                        ctx.strokeStyle = root.bgPanel;
                                        ctx.stroke();

                                        start = end;
                                    }
                                }
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 0
                                Text {
                                    text: backend.totalProcessWatts.toFixed(1) + "W"
                                    color: root.textMain
                                    font.bold: true
                                    font.pixelSize: 14
                                    font.family: "Monospace"
                                    Layout.alignment: Qt.AlignCenter
                                }
                                Text {
                                    text: "PROCESS"
                                    color: root.textMuted
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.alignment: Qt.AlignCenter
                                }
                            }
                        }

                        // Legend 2-Column Grid (Wide horizontal space utilization)
                        GridLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.alignment: Qt.AlignVCenter
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 2

                            Repeater {
                                model: backend.processPowerShares
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Rectangle { width: 7; height: 7; radius: 2; color: modelData.color }
                                    Text {
                                        text: modelData.name + (modelData.pid > 0 ? (" (" + modelData.pid + ")") : "")
                                        color: root.textMain
                                        font.pixelSize: 11
                                        font.bold: true
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.watts.toFixed(1) + "W"
                                        color: modelData.color
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                    }
                                    Text {
                                        text: modelData.pct.toFixed(0) + "%"
                                        color: root.textDim
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                        Layout.preferredWidth: 30
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =============================================================
        // 4. BOTTOM CONTROL DOCK (42px)
        // =============================================================
        Rectangle {
            Layout.fillWidth: true
            height: 42
            color: root.bgPanel
            border.color: root.borderPanel
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text { text: "PROFILES:"; color: root.textDim; font.bold: true; font.pixelSize: 11 }

                // Performance Mode (REF-REQ-067: Locked out when battery <= 20%)
                // Performance Mode (REF-REQ-067: Locked out when battery <= 20%)
                TactileButton {
                    id: btnPerf
                    text: "⚡ Performance (4.1G)"
                    highlighted: backend.powerProfileMode === 0
                    accentColor: root.colCyan
                    enabled: !(backend.batteryState === 1 && backend.batteryPercent <= 20)
                    opacity: enabled ? 1.0 : 0.4
                    onClicked: {
                        if (backend.batteryState === 1 && backend.batteryPercent <= 20) return;
                        backend.setProfile(0)
                    }
                    ToolTip.visible: hovered && (backend.batteryState === 1 && backend.batteryPercent <= 20)
                    ToolTip.text: "배터리 20% 이하에서는 배터리 보호를 위해 고성능 모드를 사용할 수 없습니다 (REF-REQ-067)"
                }

                // Balanced Mode
                TactileButton {
                    text: "⚖️ Balanced"
                    highlighted: backend.powerProfileMode === 1
                    accentColor: root.colGreen
                    onClicked: backend.setProfile(1)
                }

                // Smart Save Mode
                TactileButton {
                    text: "🌱 Smart Save (1.7G)"
                    highlighted: backend.powerProfileMode === 2
                    accentColor: root.colOrange
                    onClicked: backend.setProfile(2)
                }

                // Ultra Save Mode
                TactileButton {
                    text: "❄️ Ultra Save (1.4G)"
                    highlighted: backend.powerProfileMode === 3
                    accentColor: root.colPurple
                    onClicked: backend.setProfile(3)
                }

                Item { Layout.fillWidth: true }

                // Trigger Rescan
                TactileButton {
                    text: backend.isRescanning ? "측정 중..." : "🔄 지금 정밀 재측정"
                    accentColor: root.colCyan
                    baseColor: "#152438"
                    hoverColor: "#1d324d"
                    enabled: !backend.isRescanning
                    onClicked: backend.triggerRescan()
                }

                // System Monitor
                TactileButton {
                    text: "📊 시스템 모니터"
                    accentColor: root.colBlue
                    baseColor: "#1b212c"
                    hoverColor: "#273142"
                    onClicked: backend.openSystemMonitor()
                }

                // Close Button
                TactileButton {
                    implicitWidth: 32
                    implicitHeight: 30
                    text: "✕"
                    accentColor: root.colRed
                    baseColor: "#24181b"
                    hoverColor: "#3d1e23"
                    pressColor: "#170a0d"
                    textColor: root.colRed
                    onClicked: root.close()
                }
            }
        }
    }

    // =============================================================
    // 4. FLOATING CYBER INSPECTION CARD (REF-REQ-038)
    // =============================================================
    Rectangle {
        id: hoverCard
        visible: root.hoverVisible || opacity > 0.01
        opacity: root.hoverVisible ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 80 } }
        z: 9999

        // Dimensions
        width: root.hoverType === "process" ? 490 : 420
        height: root.hoverType === "process" ? 340 : 230

        // Pass through mouse events
        enabled: false

        // Smart Cursor-Following Tooltip Position (Near Cursor, Strictly In-Bounds)
        x: (root.hoverTargetX + width + 24 < root.width) 
           ? (root.hoverTargetX + 16) 
           : Math.max(12, root.hoverTargetX - width - 16)

        y: (root.hoverTargetY + height + 24 < root.height)
           ? (root.hoverTargetY + 16)
           : Math.max(12, root.hoverTargetY - height - 16)

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

        // PROCESS HOVER DETAILS
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6
            visible: root.hoverType === "process" && root.hoverData !== null

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    width: 28; height: 28; radius: 4
                    color: root.hoverData && root.hoverData["tier"] === 0 ? "#142921" : (root.hoverData && root.hoverData["tier"] === 5 ? "#361313" : "#1e2838")
                    border.color: root.hoverData && root.hoverData["tier"] === 0 ? root.colGreen : (root.hoverData && root.hoverData["tier"] === 5 ? root.colRed : root.colCyan)
                    Text {
                        anchors.centerIn: parent
                        text: root.hoverData ? ("T" + root.hoverData["tier"]) : "T?"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }

                ColumnLayout {
                    spacing: 1
                    Text {
                        text: root.hoverData ? (root.hoverData["comm"] + " (PID " + root.hoverData["pid"] + ", UID " + root.hoverData["uid"] + ")") : ""
                        color: root.textMain
                        font.bold: true
                        font.pixelSize: 13
                        font.family: "Monospace"
                    }
                    Text {
                        text: root.hoverData ? ("안전 등급: Tier " + root.hoverData["tier"] + " | WDI 피로 지수: " + (root.hoverData["wdiScore"] ? root.hoverData["wdiScore"].toFixed(1) : "0.0")) : ""
                        color: root.textDim
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                ColumnLayout {
                    spacing: 1
                    Text {
                        text: root.hoverData ? ((root.hoverData["totalWatts"] >= 1.0 ? root.hoverData["totalWatts"].toFixed(2) + " W" : (root.hoverData["totalWatts"] * 1000).toFixed(0) + " mW")) : ""
                        color: root.colOrange
                        font.bold: true
                        font.pixelSize: 16
                        font.family: "Monospace"
                        Layout.alignment: Qt.AlignRight
                    }
                    Text {
                        text: root.hoverData ? ("기여율: " + (root.hoverData["ratioPercent"] ? root.hoverData["ratioPercent"].toFixed(1) : "0") + "%") : ""
                        color: root.textMuted
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            Text { text: "⚡ 물리 하드웨어 도메인별 전력 분해 (Attributed Power):"; color: root.colCyan; font.bold: true; font.pixelSize: 11 }

            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 3
                columnSpacing: 8

                Text { text: "💻 CPU 연산: " + (root.hoverData ? (root.hoverData["cpuWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.textMain; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "🎮 GPU 실리콘: " + (root.hoverData ? (root.hoverData["gpuWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.colGreen; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "🧠 DRAM 버스: " + (root.hoverData ? (root.hoverData["dramWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.colPurple; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "⚡ 웨이크업 벌금: " + (root.hoverData ? (root.hoverData["wakeTaxWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.colOrange; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "💾 디스크 I/O: " + (root.hoverData ? (root.hoverData["ioWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.textDim; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "🌪️ 유도 팬 전력: " + (root.hoverData ? (root.hoverData["fanWatts"] * 1000).toFixed(0) : "0") + " mW"; color: root.textDim; font.pixelSize: 11; font.family: "Monospace" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            Text { text: "⚙️ 스케줄러 및 CPU 실행 프로필:"; color: root.colCyan; font.bold: true; font.pixelSize: 11 }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "고정 코어: Core #" + (root.hoverData && root.hoverData["cpuCore"] >= 0 ? root.hoverData["cpuCore"] : "All") + (root.hoverData && root.hoverData["crossCcx"] === 1 ? " (!CCX이동)" : "")
                    color: root.hoverData && root.hoverData["crossCcx"] === 1 ? root.colRed : root.textMain
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text { text: "스레드: " + (root.hoverData ? root.hoverData["threads"] : 1) + "개"; color: root.textDim; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: "Nice/Pri: " + (root.hoverData ? root.hoverData["nice"] : 0) + " / " + (root.hoverData ? root.hoverData["priority"] : 20); color: root.textDim; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: "웨이크업: " + (root.hoverData ? root.hoverData["wakeupsSec"] : 0) + "/s"; color: root.colOrange; font.pixelSize: 11 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            Text { text: "💾 메모리 & VFS I/O 상태:"; color: root.colCyan; font.bold: true; font.pixelSize: 11 }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "정격 PSS: " + (root.hoverData ? root.hoverData["pssMb"] : 0) + " MB"; color: root.textMain; font.pixelSize: 11; font.family: "Monospace" }
                Item { Layout.fillWidth: true }
                Text { text: "VRAM: " + (root.hoverData ? (root.hoverData["vramMb"] ? root.hoverData["vramMb"].toFixed(0) : "0") : "0") + " MB"; color: root.colGreen; font.pixelSize: 11; font.family: "Monospace" }
                Item { Layout.fillWidth: true }
                Text { text: "네트워크: " + (root.hoverData ? root.hoverData["openSockets"] : 0) + " Sockets"; color: root.colBlue; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: "I/O: " + (root.hoverData ? (root.hoverData["ioMbSec"] ? root.hoverData["ioMbSec"].toFixed(2) : "0.0") : "0.0") + " MB/s"; color: root.textDim; font.pixelSize: 11 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text { text: "🔍 WattCurb 원인 규명 및 진단 메커니즘:"; color: root.colOrange; font.bold: true; font.pixelSize: 11 }
                Text {
                    text: root.hoverData ? ("[" + root.hoverData["domain"] + "] " + root.hoverData["mechanism"]) : ""
                    color: root.textMain
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // CPU HOVER DETAILS
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6
            visible: root.hoverType === "cpu"

            RowLayout {
                Text { text: "💻 CPU RAPL & PMU 하드웨어 성능 카운터 심층 텔레메트리"; color: root.colCyan; font.bold: true; font.pixelSize: 12 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 4
                columnSpacing: 10

                Text { text: "CPU 주파수 거버너: " + backend.cpuGovernor; color: root.textMain; font.pixelSize: 11 }
                Text { text: "PMU IPC (명령어/사이클): " + backend.pmuIpc.toFixed(2); color: root.colCyan; font.bold: true; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "총 실행 명령어: " + backend.pmuInstructions.toLocaleString(); color: root.textDim; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "CPU 클럭 사이클: " + backend.pmuCycles.toLocaleString(); color: root.textDim; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "LLC 캐시 미스: " + backend.pmuLlcMisses.toLocaleString(); color: root.colOrange; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "분기 예측 실패: " + backend.pmuBranchMisses.toLocaleString(); color: root.colOrange; font.pixelSize: 11; font.family: "Monospace" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Energy Waste Ratio (EWR): " + backend.pmuEwr.toFixed(1) + "%"; color: backend.pmuEwr > 20 ? root.colRed : root.colGreen; font.bold: true; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: "Zero-Wakeup Active"; color: root.textMuted; font.pixelSize: 10 }
            }
        }

        // BATTERY HOVER DETAILS
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6
            visible: root.hoverType === "battery"

            RowLayout {
                Text { text: "🔋 BAT0 배터리 화학 및 전기적 물리 사양 심층 텔레메트리"; color: root.colGreen; font.bold: true; font.pixelSize: 12 }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 4
                columnSpacing: 10

                Text { text: "배터리 셀 제조사: " + backend.batteryMfg; color: root.textMain; font.pixelSize: 11 }
                Text { text: "배터리 모델명: " + backend.batteryModel; color: root.textMain; font.pixelSize: 11 }
                Text { text: "배터리 화학 기술: " + backend.batteryTech; color: root.textDim; font.pixelSize: 11 }
                Text { text: "완충 사이클: " + backend.batteryCycles + " 회"; color: root.textDim; font.pixelSize: 11 }
                Text { text: "설계 용량 (Design): " + backend.batteryDesignWh.toFixed(2) + " Wh"; color: root.textDim; font.pixelSize: 11; font.family: "Monospace" }
                Text { text: "현재 만충 용량 (Full): " + backend.batteryFullWh.toFixed(2) + " Wh"; color: root.colGreen; font.bold: true; font.pixelSize: 11; font.family: "Monospace" }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#222a36" }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "배터리 건강도: " + backend.batteryHealth + "% (열화율 " + (100 - backend.batteryHealth) + "%)"; color: root.colGreen; font.bold: true; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: backend.batteryState === 2 ? "AC Pass-through: ON" : "Discharging"; color: root.colCyan; font.pixelSize: 11; font.bold: true }
            }
        }
    }
}
