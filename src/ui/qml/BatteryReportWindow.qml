import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: reportWin
    visible: false
    width: 1180
    height: 780
    minimumWidth: 1000
    minimumHeight: 660
    title: "WattCurb 배터리 심층 드레인 전수 분석 리포트 (Deep Battery Drain Telemetry Audit)"
    color: "#0b0e12"

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

    property string copyToastText: ""
    property bool showCopyToast: false

    Timer {
        id: toastTimer
        interval: 2500
        onTriggered: reportWin.showCopyToast = false
    }

    // =========================================================================
    // REUSABLE TACTILE CYBER BUTTON COMPONENT (REF-REQ-081, REF-ARCH-058)
    // Physical depression (scale: 0.95), distinct hover lighting, and border glow
    // =========================================================================
    component TactileButton: Button {
        id: tBtn
        property color accentColor: reportWin.colCyan
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // =============================================================
        // 1. REPORT HEADER & ACTION BAR
        // =============================================================
        Rectangle {
            Layout.fillWidth: true
            height: 52
            color: reportWin.bgPanel
            border.color: reportWin.borderPanel
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Rectangle {
                    width: 30; height: 30; radius: 6; color: "#0284c7"
                    Text { anchors.centerIn: parent; text: "🔋"; font.pixelSize: 18 }
                }

                ColumnLayout {
                    spacing: 1
                    Text {
                        text: "BATTERY DRAIN DEEP AUDIT REPORT"
                        color: reportWin.colCyan
                        font.bold: true
                        font.pixelSize: 15
                        font.family: "Monospace"
                    }
                    Text {
                        text: "누적 방전 로그 전수 분석 (" + (backend.batteryReportSummary.durationStr || "전체 히스토리") + " · " + (backend.batteryReportSummary.totalSamples || 0) + "개 샘플)"
                        color: reportWin.textDim
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                // Copy Toast notification
                Rectangle {
                    visible: reportWin.showCopyToast
                    height: 28
                    Layout.preferredWidth: toastLabel.implicitWidth + 20
                    color: "#065f46"
                    border.color: reportWin.colGreen
                    radius: 4
                    Text {
                        id: toastLabel
                        anchors.centerIn: parent
                        text: reportWin.copyToastText
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                // Action 1: Refresh
                TactileButton {
                    text: "🔄 새로고침"
                    accentColor: reportWin.colCyan
                    baseColor: "#13212f"
                    hoverColor: "#1c3249"
                    textColor: reportWin.colCyan
                    onClicked: backend.generateBatteryReport()
                }

                // Action 2: Copy Markdown Report
                TactileButton {
                    text: "📋 리포트 마크다운 복사"
                    accentColor: reportWin.colGreen
                    baseColor: "#0f3327"
                    hoverColor: "#154d3b"
                    pressColor: "#072017"
                    textColor: "#34d399"
                    onClicked: {
                        backend.copyReportToClipboard();
                        reportWin.copyToastText = "✓ 마크다운 리포트가 클립보드에 복사되었습니다!";
                        reportWin.showCopyToast = true;
                        toastTimer.restart();
                    }
                }

                // Action 3: Close
                TactileButton {
                    text: "닫기"
                    accentColor: reportWin.colRed
                    baseColor: "#221a1d"
                    hoverColor: "#382228"
                    pressColor: "#140c0f"
                    textColor: "#fca5a5"
                    onClicked: reportWin.close()
                }
            }
        }

        // =============================================================
        // 2. 4 EXECUTIVE KPI CARDS
        // =============================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            // CARD 1: Total Discharged Energy
            Rectangle {
                Layout.fillWidth: true
                height: 86
                color: reportWin.bgPanel
                border.color: reportWin.borderPanel
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text { text: "⚡ 총 방전 소모량 (Total Discharge)"; color: reportWin.textDim; font.pixelSize: 11; font.bold: true }
                    Text {
                        text: (backend.batteryReportSummary.totalDischargeWh ? backend.batteryReportSummary.totalDischargeWh.toFixed(2) : "0.00") + " Wh"
                        color: reportWin.colCyan
                        font.bold: true
                        font.pixelSize: 22
                        font.family: "Monospace"
                    }
                    Text {
                        text: (backend.batteryReportSummary.totalDischargeMah ? backend.batteryReportSummary.totalDischargeMah.toFixed(0) : "0") + " mAh · Δ " + (backend.batteryReportSummary.batteryDropPct || 0) + "% (" + (backend.batteryReportSummary.batteryStartPct || 0) + "% → " + (backend.batteryReportSummary.batteryEndPct || 0) + "%)"
                        color: reportWin.textMuted
                        font.pixelSize: 10
                    }
                }
            }

            // CARD 2: Average Discharge Rate
            Rectangle {
                Layout.fillWidth: true
                height: 86
                color: reportWin.bgPanel
                border.color: reportWin.borderPanel
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text { text: "📊 평균 방전율 (Avg Discharge Rate)"; color: reportWin.textDim; font.pixelSize: 11; font.bold: true }
                    Text {
                        text: (backend.batteryReportSummary.avgDischargeWatts ? backend.batteryReportSummary.avgDischargeWatts.toFixed(2) : "0.00") + " W"
                        color: (backend.batteryReportSummary.avgDischargeWatts > 18.0) ? reportWin.colRed : ((backend.batteryReportSummary.avgDischargeWatts > 12.0) ? reportWin.colOrange : reportWin.colGreen)
                        font.bold: true
                        font.pixelSize: 22
                        font.family: "Monospace"
                    }
                    Text {
                        text: (backend.batteryReportSummary.avgDischargeWatts > 18.0) ? "주의: 배터리 소모율이 매우 높습니다" : "안정적인 방전 속도 유지 중"
                        color: reportWin.textMuted
                        font.pixelSize: 10
                    }
                }
            }

            // CARD 3: Peak Discharge
            Rectangle {
                Layout.fillWidth: true
                height: 86
                color: reportWin.bgPanel
                border.color: reportWin.borderPanel
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text { text: "📈 최대 피크 전력 (Peak Discharge)"; color: reportWin.textDim; font.pixelSize: 11; font.bold: true }
                    Text {
                        text: (backend.batteryReportSummary.peakDischargeWatts ? backend.batteryReportSummary.peakDischargeWatts.toFixed(2) : "0.00") + " W"
                        color: reportWin.colOrange
                        font.bold: true
                        font.pixelSize: 22
                        font.family: "Monospace"
                    }
                    Text {
                        text: backend.batteryReportSummary.peakTimeStr ? ("발생 시각: " + backend.batteryReportSummary.peakTimeStr) : "순간 최대 전력"
                        color: reportWin.textMuted
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            // CARD 4: Deep Sleep & Temp
            Rectangle {
                Layout.fillWidth: true
                height: 86
                color: reportWin.bgPanel
                border.color: reportWin.borderPanel
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 2
                    Text { text: "💤 Deep Sleep 효율 (C3+ / Temp)"; color: reportWin.textDim; font.pixelSize: 11; font.bold: true }
                    Text {
                        text: (backend.batteryReportSummary.avgCstateC3Percent ? backend.batteryReportSummary.avgCstateC3Percent.toFixed(1) : "0.0") + "%"
                        color: (backend.batteryReportSummary.avgCstateC3Percent >= 70.0) ? reportWin.colGreen : reportWin.colOrange
                        font.bold: true
                        font.pixelSize: 22
                        font.family: "Monospace"
                    }
                    Text {
                        text: "평균 온도: " + (backend.batteryReportSummary.avgCpuTempC ? backend.batteryReportSummary.avgCpuTempC.toFixed(1) : "0") + " °C · 슬립 방해 분석"
                        color: reportWin.textMuted
                        font.pixelSize: 10
                    }
                }
            }
        }

        // =============================================================
        // 3. MAIN SPLIT: HARDWARE DOMAINS (LEFT) & TOP PROCESS CULPRITS (RIGHT)
        // =============================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // ---------------------------------------------------------
            // LEFT: PHYSICAL HARDWARE DOMAIN BREAKDOWN (420px)
            // ---------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 420
                Layout.minimumWidth: 380
                Layout.maximumWidth: 460
                Layout.fillHeight: true
                color: reportWin.bgPanel
                border.color: reportWin.borderPanel
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Text { text: "🔬 물리 하드웨어 도메인별 방전 기여도"; color: reportWin.colCyan; font.bold: true; font.pixelSize: 13 }
                        Item { Layout.fillWidth: true }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: reportWin.borderPanel }

                    // Domain list
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: backend.batteryReportHardwareShares

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 68
                            color: reportWin.bgRowAlt
                            border.color: reportWin.borderPanel
                            radius: 4

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 4

                                RowLayout {
                                    spacing: 6
                                    Text { text: modelData.icon || "⚙️"; font.pixelSize: 13 }
                                    Text {
                                        text: modelData.name || ""
                                        color: reportWin.textMain
                                        font.bold: true
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: (modelData.wh ? modelData.wh.toFixed(2) : "0.00") + " Wh"
                                        color: modelData.color || reportWin.colCyan
                                        font.bold: true
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                    }
                                    Text {
                                        text: "(" + (modelData.percent ? modelData.percent.toFixed(1) : "0.0") + "%)"
                                        color: reportWin.textDim
                                        font.pixelSize: 11
                                        font.family: "Monospace"
                                    }
                                }

                                // Visual Progress Bar
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 5
                                    radius: 2
                                    color: "#1e293b"
                                    Rectangle {
                                        width: parent.width * Math.min(1.0, (modelData.percent || 0) / 100.0)
                                        height: parent.height
                                        radius: 2
                                        color: modelData.color || reportWin.colCyan
                                    }
                                }

                                Text {
                                    text: modelData.tip || ""
                                    color: reportWin.textMuted
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------
            // RIGHT: TOP BATTERY DRAIN CULPRITS (PROCESSES)
            // ---------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: reportWin.bgPanel
                border.color: reportWin.borderPanel
                radius: 6

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Text { text: "🚨 배터리 드레인 주범 프로세스 순위 (Top Drain Culprits)"; color: reportWin.colOrange; font.bold: true; font.pixelSize: 13 }
                        Item { Layout.fillWidth: true }
                        Text { text: "WattCurb 하드웨어 인과 추적 엔진"; color: reportWin.textMuted; font.pixelSize: 11 }
                    }

                    // Table Header
                    Rectangle {
                        Layout.fillWidth: true
                        height: 26
                        color: reportWin.bgPanelHeader
                        radius: 4

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            Text { text: "순위"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 32 }
                            Text { text: "프로세스명 (PID)"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 200 }
                            Text { text: "하드웨어 도메인"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 120 }
                            Text { text: "추정 방전량"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 85; horizontalAlignment: Text.AlignRight }
                            Text { text: "평균 전력"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                            Text { text: "부하 비중"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.preferredWidth: 65; horizontalAlignment: Text.AlignRight }
                            Text { text: "유발 메커니즘 & 권장 조치"; color: reportWin.textDim; font.bold: true; font.pixelSize: 11; Layout.fillWidth: true }
                        }
                    }

                    // Culprits List
                    ListView {
                        id: culpritList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: backend.batteryReportProcessCulprits

                        delegate: Rectangle {
                            id: rowDelegate
                            width: culpritList.width
                            height: 34
                            color: rowMa.containsMouse ? "#1f2937" : (index % 2 === 1 ? reportWin.bgRowAlt : "transparent")
                            radius: 3

                            MouseArea {
                                id: rowMa
                                anchors.fill: parent
                                hoverEnabled: true
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8

                                // Rank Badge
                                Rectangle {
                                    Layout.preferredWidth: 26
                                    height: 20
                                    radius: 3
                                    color: index === 0 ? "#7f1d1d" : (index === 1 ? "#78350f" : "#1e293b")
                                    Text {
                                        anchors.centerIn: parent
                                        text: "#" + modelData.rank
                                        color: index === 0 ? reportWin.colRed : (index === 1 ? reportWin.colOrange : reportWin.colCyan)
                                        font.bold: true
                                        font.pixelSize: 10
                                    }
                                }

                                // Process comm + PID with Interactive Cyber ToolTip (REF-REQ-083, REF-ARCH-060)
                                Item {
                                    Layout.preferredWidth: 200
                                    Layout.fillHeight: true

                                    MouseArea {
                                        id: procMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        ToolTip.visible: procMa.containsMouse
                                        ToolTip.delay: 150
                                        ToolTip.timeout: 10000
                                        ToolTip.background: Rectangle {
                                            color: "#0f172a"
                                            border.color: reportWin.colCyan
                                            border.width: 1.5
                                            radius: 6

                                            Rectangle {
                                                anchors.fill: parent
                                                anchors.margins: -1
                                                radius: 6
                                                color: "transparent"
                                                border.color: "#3300d2ff"
                                                border.width: 1
                                                opacity: 0.5
                                            }
                                        }
                                        ToolTip.contentItem: ColumnLayout {
                                            spacing: 4
                                            RowLayout {
                                                spacing: 6
                                                Rectangle { width: 4; height: 14; radius: 2; color: reportWin.colCyan }
                                                Text {
                                                    text: modelData.fullName || modelData.comm || ""
                                                    color: "#ffffff"
                                                    font.bold: true
                                                    font.pixelSize: 12
                                                    font.family: "Monospace"
                                                }
                                                Text {
                                                    text: "(PID " + modelData.pid + ")"
                                                    color: reportWin.colOrange
                                                    font.pixelSize: 11
                                                    font.family: "Monospace"
                                                }
                                            }
                                            Text {
                                                visible: !!modelData.cmdline && modelData.cmdline !== (modelData.fullName || modelData.comm)
                                                text: modelData.cmdline || ""
                                                color: reportWin.textDim
                                                font.pixelSize: 10
                                                font.family: "Monospace"
                                                wrapMode: Text.WrapAnywhere
                                                Layout.maximumWidth: 440
                                            }
                                            Rectangle {
                                                Layout.fillWidth: true
                                                height: 1
                                                color: reportWin.borderPanel
                                            }
                                            RowLayout {
                                                spacing: 12
                                                Text {
                                                    text: "도메인: " + (modelData.domain || "CPU")
                                                    color: reportWin.colCyan
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "방전량: " + (modelData.drainWh ? modelData.drainWh.toFixed(2) : "0.00") + " Wh (" + (modelData.sharePercent ? modelData.sharePercent.toFixed(1) : "0.0") + "%)"
                                                    color: reportWin.colOrange
                                                    font.bold: true
                                                    font.pixelSize: 10
                                                }
                                            }
                                            Text {
                                                text: "메커니즘: " + (modelData.mechanism || "Normal execution")
                                                color: reportWin.textMuted
                                                font.pixelSize: 9
                                            }
                                        }
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 4
                                        Text {
                                            text: modelData.fullName || modelData.comm || ""
                                            color: procMa.containsMouse ? reportWin.colCyan : reportWin.textMain
                                            font.bold: true
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: "(" + modelData.pid + ")"
                                            color: procMa.containsMouse ? reportWin.colCyan : reportWin.textMuted
                                            font.pixelSize: 10
                                            font.family: "Monospace"
                                        }
                                    }
                                }

                                // Domain
                                Rectangle {
                                    Layout.preferredWidth: 120
                                    height: 20
                                    radius: 3
                                    color: "#161d27"
                                    border.color: reportWin.borderPanel
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.domain || "CPU Compute"
                                        color: reportWin.colCyan
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }

                                // Drain Wh
                                Text {
                                    Layout.preferredWidth: 85
                                    text: (modelData.drainWh ? modelData.drainWh.toFixed(2) : "0.00") + " Wh"
                                    color: reportWin.colOrange
                                    font.bold: true
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    horizontalAlignment: Text.AlignRight
                                }

                                // Avg Watts
                                Text {
                                    Layout.preferredWidth: 70
                                    text: (modelData.avgWatts ? modelData.avgWatts.toFixed(2) : "0.00") + " W"
                                    color: reportWin.textMain
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    horizontalAlignment: Text.AlignRight
                                }

                                // Share %
                                Text {
                                    Layout.preferredWidth: 65
                                    text: (modelData.sharePercent ? modelData.sharePercent.toFixed(1) : "0.0") + "%"
                                    color: modelData.sharePercent > 30 ? reportWin.colRed : reportWin.textDim
                                    font.bold: modelData.sharePercent > 30
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    horizontalAlignment: Text.AlignRight
                                }

                                // Mechanism & Action
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true

                                        MouseArea {
                                            id: mechMa
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            ToolTip.visible: mechMa.containsMouse && (modelData.mechanism && modelData.mechanism.length > 25)
                                            ToolTip.delay: 250
                                            ToolTip.text: modelData.mechanism || ""
                                        }

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width
                                            text: modelData.mechanism || "Normal execution"
                                            color: mechMa.containsMouse ? reportWin.textMain : reportWin.textDim
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Rectangle {
                                        height: 18
                                        Layout.preferredWidth: actionText.implicitWidth + 10
                                        radius: 3
                                        color: "#1e293b"
                                        border.color: reportWin.borderPanel
                                        Text {
                                            id: actionText
                                            anchors.centerIn: parent
                                            text: modelData.action || "Monitor"
                                            color: reportWin.colGreen
                                            font.pixelSize: 9
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // =============================================================
        // 4. DIAGNOSTIC SUMMARY & ACTIONABLE RECOMMENDATIONS BOX
        // =============================================================
        Rectangle {
            Layout.fillWidth: true
            height: 110
            color: reportWin.bgPanel
            border.color: reportWin.borderPanel
            radius: 6

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4

                    RowLayout {
                        Text { text: "🧠 WattCurb 인공지능/엔진 배터리 드레인 종합 진단"; color: reportWin.colCyan; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        text: backend.batteryReportSummary.diagnosticSummary || "진단 데이터를 분석 중입니다..."
                        color: reportWin.textMain
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: reportWin.borderPanel }

                    Text {
                        text: backend.batteryReportSummary.recommendationText || "권장 최적화 지침을 로드 중입니다."
                        color: reportWin.colGreen
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }

                // Quick Action: Switch to Ultra Battery
                ColumnLayout {
                    Layout.preferredWidth: 160
                    Layout.fillHeight: true
                    spacing: 4

                    Item { Layout.fillHeight: true }

                    TactileButton {
                        Layout.fillWidth: true
                        implicitHeight: 36
                        text: "⚡ Ultra Battery 전환"
                        accentColor: reportWin.colGreen
                        baseColor: "#0f3a2c"
                        hoverColor: "#155742"
                        pressColor: "#07241a"
                        textColor: "#34d399"
                        onClicked: {
                            backend.setProfile(3); // Mode 3: Ultra Battery
                            reportWin.copyToastText = "✓ Ultra Battery 모드로 즉시 전환되었습니다!";
                            reportWin.showCopyToast = true;
                            toastTimer.restart();
                        }
                    }

                    Text {
                        text: "배경 프로세스 강제 동결"
                        color: reportWin.textMuted
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
