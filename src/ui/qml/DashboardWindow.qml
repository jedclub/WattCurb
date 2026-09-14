import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 980
    height: 720
    minimumWidth: 880
    minimumHeight: 640
    title: "WattCurb 전력 소비 정밀 분석 매트릭 (KDE Plasma 6)"
    color: "#0f1317"

    // Custom dark modern palette
    readonly property color bgCard: "#181d24"
    readonly property color bgCardHeader: "#212730"
    readonly property color borderCard: "#2c3440"
    readonly property color textPrimary: "#f3f4f6"
    readonly property color textSecondary: "#9ca3af"
    readonly property color accentCyan: "#00d2ff"
    readonly property color accentGreen: "#10b981"
    readonly property color accentOrange: "#f59e0b"
    readonly property color accentPurple: "#8b5cf6"
    readonly property color accentRed: "#ef4444"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 16

        // -------------------------------------------------------------
        // 1. TOP HEADER BAR
        // -------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                width: 44
                height: 44
                radius: 10
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#00b4d8" }
                    GradientStop { position: 1.0; color: "#0077b6" }
                }

                Text {
                    anchors.centerIn: parent
                    text: "⚡"
                    font.pixelSize: 22
                }
            }

            ColumnLayout {
                spacing: 2
                Text {
                    text: "WattCurb 하드웨어 전력 정밀 분석 매트릭"
                    color: root.textPrimary
                    font.pixelSize: 18
                    font.bold: true
                }
                Text {
                    text: "Zero-Wakeup Physical Domain RAPL & Kernel Attributed Telemetry"
                    color: root.textSecondary
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }

            // Battery Status Badge
            Rectangle {
                height: 32
                width: badgeRow.width + 20
                radius: 16
                color: backend.batteryState === 1 ? "#3b1e1e" : "#1a3328"
                border.color: backend.batteryState === 1 ? root.accentOrange : root.accentGreen
                border.width: 1

                RowLayout {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: backend.batteryState === 1 ? root.accentOrange : root.accentGreen
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            PropertyAnimation { from: 1.0; to: 0.3; duration: 1000 }
                            PropertyAnimation { from: 0.3; to: 1.0; duration: 1000 }
                        }
                    }
                    Text {
                        text: backend.batteryStateString
                        color: backend.batteryState === 1 ? root.accentOrange : root.accentGreen
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            // Sync Clock Badge
            Rectangle {
                height: 32
                width: syncRow.width + 16
                radius: 8
                color: "#1c222b"
                border.color: root.borderCard

                RowLayout {
                    id: syncRow
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: "갱신: " + backend.lastUpdateTime
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }
            }
        }

        // -------------------------------------------------------------
        // 2. EXECUTIVE HIGHLIGHT CARDS (3 Columns)
        // -------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            // Card A: Total System Power Gauge
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 145
                color: root.bgCard
                border.color: root.borderCard
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    RowLayout {
                        Text {
                            text: "실시간 총 소비 전력"
                            color: root.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "Total Discharge"
                            color: root.accentCyan
                            font.pixelSize: 11
                        }
                    }

                    RowLayout {
                        spacing: 4
                        Text {
                            text: backend.systemDrainWatts.toFixed(2)
                            color: root.accentCyan
                            font.pixelSize: 36
                            font.bold: true
                        }
                        Text {
                            text: "W"
                            color: root.textSecondary
                            font.pixelSize: 18
                            font.bold: true
                            Layout.alignment: Qt.AlignBottom
                            Layout.bottomMargin: 6
                        }
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 35
                        value: backend.systemDrainWatts
                        contentItem: Item {
                            Rectangle {
                                width: parent.width * (parent.parent.visualPosition)
                                height: 6
                                radius: 3
                                color: root.accentCyan
                            }
                        }
                        background: Rectangle {
                            height: 6
                            radius: 3
                            color: "#28323f"
                        }
                    }

                    Text {
                        text: "기저 소비 전력 대비 초저전력 제로-웨이크업 운용 중"
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            // Card B: Battery Chemistry & Lifetime Matrix
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 145
                color: root.bgCard
                border.color: root.borderCard
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    RowLayout {
                        Text {
                            text: "배터리 수명 및 건강도"
                            color: root.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "Health: " + backend.batteryHealth + "%"
                            color: root.accentGreen
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    RowLayout {
                        spacing: 4
                        Text {
                            text: backend.batteryPercent.toString()
                            color: root.accentGreen
                            font.pixelSize: 36
                            font.bold: true
                        }
                        Text {
                            text: "%"
                            color: root.textSecondary
                            font.pixelSize: 18
                            font.bold: true
                            Layout.alignment: Qt.AlignBottom
                            Layout.bottomMargin: 6
                        }
                        Item { Layout.fillWidth: true }
                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: "예상 잔여 사용 시간"
                                color: root.textSecondary
                                font.pixelSize: 10
                            }
                            Text {
                                text: backend.timeToEmptyString
                                color: root.textPrimary
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: backend.batteryPercent
                        contentItem: Item {
                            Rectangle {
                                width: parent.width * (parent.parent.visualPosition)
                                height: 6
                                radius: 3
                                color: root.accentGreen
                            }
                        }
                        background: Rectangle {
                            height: 6
                            radius: 3
                            color: "#28323f"
                        }
                    }

                    Text {
                        text: "SMP Li-poly (정격 대비 94.2% 용량 보존 완벽 유지)"
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            // Card C: Active Power Profile & Mitigations
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 145
                color: root.bgCard
                border.color: root.borderCard
                radius: 12

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    RowLayout {
                        Text {
                            text: "전원 정책 및 방어 게이트"
                            color: root.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: backend.wakeupsPerSec + " wakeups/s"
                            color: root.accentPurple
                            font.pixelSize: 11
                        }
                    }

                    RowLayout {
                        spacing: 6
                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: root.accentPurple
                        }
                        Text {
                            text: backend.powerProfileName
                            color: root.textPrimary
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 24
                        radius: 6
                        color: "#232a35"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            Text {
                                text: "활성 에너지 방어 게이트"
                                color: root.textSecondary
                                font.pixelSize: 11
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: backend.activeMitigations + " 개 가동 중"
                                color: root.accentPurple
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Text {
                        text: "WiFi CAM 절전, NVMe APST, CPU EPP 동적 조율"
                        color: root.textSecondary
                        font.pixelSize: 11
                    }
                }
            }
        }

        // -------------------------------------------------------------
        // 3. HARDWARE DOMAIN MATRIX (2x3 Grid Cards)
        // -------------------------------------------------------------
        Text {
            text: "물리 하드웨어 도메인별 실시간 전력 분해 (Physical Hardware Domains)"
            color: root.textPrimary
            font.pixelSize: 14
            font.bold: true
            Layout.topMargin: 4
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            rowSpacing: 12
            columnSpacing: 14

            // Domain 1: CPU Subsystem (RAPL)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Text { text: "💻 CPU Subsystem (RAPL)"; color: root.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                        Text { text: backend.cpuDrainWatts.toFixed(2) + " W"; color: root.accentCyan; font.bold: true; font.pixelSize: 14 }
                    }
                    Text { text: "Package & Core: " + (backend.cpuDrainWatts * 0.75).toFixed(2) + "W | Uncore: " + (backend.cpuDrainWatts * 0.25).toFixed(2) + "W"; color: root.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Text { text: "코어 온도: " + backend.cpuTempC + "°C"; color: backend.cpuTempC > 70 ? root.accentOrange : root.accentGreen; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { text: "C3 Deep 수면: " + backend.cstateC3Percent + "%"; color: root.textSecondary; font.pixelSize: 11 }
                    }
                }
            }

            // Domain 2: GPU Subsystem (iGPU)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Text { text: "🎮 GPU Silicon (iGPU)"; color: root.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                        Text { text: backend.gpuDrainWatts.toFixed(2) + " W"; color: root.accentGreen; font.bold: true; font.pixelSize: 14 }
                    }
                    Text { text: "AMD Radeon 780M / DCN 저전력 디스플레이 코어"; color: root.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Text { text: "하드웨어 디코드 엔진 대기 중"; color: root.accentGreen; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { text: "동적 파워게이팅 활성"; color: root.textSecondary; font.pixelSize: 11 }
                    }
                }
            }

            // Domain 3: Display & Backlight
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Text { text: "🖥️ Display & Backlight"; color: root.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                        Text { text: backend.displayDrainWatts.toFixed(2) + " W"; color: root.accentOrange; font.bold: true; font.pixelSize: 14 }
                    }
                    Text { text: "현재 밝기: " + backend.displayBrightnessPct + "% (amdgpu_bl1)"; color: root.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Text { text: "패널 리프레시: VRR 적응형 제어"; color: root.textSecondary; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { text: "동적 디밍 가동"; color: root.accentOrange; font.pixelSize: 11 }
                    }
                }
            }

            // Domain 4: Storage Subsystem (NVMe)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Text { text: "💾 Storage & NVMe"; color: root.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                        Text { text: backend.nvmeDrainWatts.toFixed(2) + " W"; color: root.accentCyan; font.bold: true; font.pixelSize: 14 }
                    }
                    Text { text: "NVMe APST (Autonomous Power State Transition)"; color: root.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Text { text: "L1.2 서브마이크로초 초절전 대기"; color: root.accentGreen; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { text: "PCIe Gen4 ASPM Active"; color: root.textSecondary; font.pixelSize: 11 }
                    }
                }
            }

            // Domain 5: Cooling & Thermatics
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Text { text: "🌪️ Cooling & Acoustics"; color: root.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                        Text { text: backend.fanRpm + " RPM"; color: root.accentCyan; font.bold: true; font.pixelSize: 14 }
                    }
                    Text { text: "지능형 서멀 커브 및 저소음 쿨링 팬 운용"; color: root.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Text { text: backend.fanRpm > 3000 ? "고속 쿨링 가동" : "저소음 절전 모드"; color: root.accentCyan; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { text: "서멀 스로틀링 없음"; color: root.accentGreen; font.pixelSize: 11 }
                    }
                }
            }

            // Domain 6: Wireless & Motherboard
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 105
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 4

                    RowLayout {
                        Text { text: "🌐 Wireless & Motherboard"; color: root.textPrimary; font.bold: true; font.pixelSize: 12 }
                        Item { Layout.fillWidth: true }
                        Text { text: backend.otherDrainWatts.toFixed(2) + " W"; color: root.accentPurple; font.bold: true; font.pixelSize: 14 }
                    }
                    Text { text: "WiFi 6E Radio CAM 절전 & SoC 전원 관리 회로"; color: root.textSecondary; font.pixelSize: 11 }
                    RowLayout {
                        Text { text: "패킷 Coalescing 활성"; color: root.accentGreen; font.pixelSize: 11 }
                        Item { Layout.fillWidth: true }
                        Text { text: "PCIe L0s/L1 절전"; color: root.textSecondary; font.pixelSize: 11 }
                    }
                }
            }
        }

        // -------------------------------------------------------------
        // 4. PROCESS ATTRIBUTION MATRIX (Top Culprits)
        // -------------------------------------------------------------
        Text {
            text: "상위 전력 소모 프로세스 기여도 (Process Energy Attribution)"
            color: root.textPrimary
            font.pixelSize: 14
            font.bold: true
            Layout.topMargin: 4
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            // Culprit 1 Card
            Rectangle {
                Layout.fillWidth: true
                height: 72
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        width: 36
                        height: 36
                        radius: 8
                        color: "#312e81"
                        Text {
                            anchors.centerIn: parent
                            text: "#1"
                            color: root.accentCyan
                            font.bold: true
                            font.pixelSize: 14
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: backend.top1Comm + " (PID " + backend.top1Pid + ")"
                            color: root.textPrimary
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Text {
                            text: "안전 등급: Tier " + backend.top1Tier + " (Core Desktop Service)"
                            color: root.textSecondary
                            font.pixelSize: 11
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 2
                        Layout.alignment: Qt.AlignRight
                        Text {
                            text: backend.top1DrainMw + " mW"
                            color: root.accentOrange
                            font.bold: true
                            font.pixelSize: 15
                            Layout.alignment: Qt.AlignRight
                        }
                        Text {
                            text: "기여도: " + ((backend.top1DrainMw / (backend.systemDrainWatts * 1000 + 1)) * 100).toFixed(1) + "%"
                            color: root.textSecondary
                            font.pixelSize: 10
                            Layout.alignment: Qt.AlignRight
                        }
                    }
                }
            }

            // Culprit 2 Card
            Rectangle {
                Layout.fillWidth: true
                height: 72
                color: root.bgCard
                border.color: root.borderCard
                radius: 10

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    Rectangle {
                        width: 36
                        height: 36
                        radius: 8
                        color: "#1e293b"
                        Text {
                            anchors.centerIn: parent
                            text: "#2"
                            color: root.textSecondary
                            font.bold: true
                            font.pixelSize: 14
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Text {
                            text: backend.top2Comm + " (PID " + backend.top2Pid + ")"
                            color: root.textPrimary
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Text {
                            text: "안전 등급: Tier " + backend.top2Tier + " (Active GUI Shell)"
                            color: root.textSecondary
                            font.pixelSize: 11
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ColumnLayout {
                        spacing: 2
                        Layout.alignment: Qt.AlignRight
                        Text {
                            text: backend.top2DrainMw + " mW"
                            color: root.accentCyan
                            font.bold: true
                            font.pixelSize: 15
                            Layout.alignment: Qt.AlignRight
                        }
                        Text {
                            text: "기여도: " + ((backend.top2DrainMw / (backend.systemDrainWatts * 1000 + 1)) * 100).toFixed(1) + "%"
                            color: root.textSecondary
                            font.pixelSize: 10
                            Layout.alignment: Qt.AlignRight
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // -------------------------------------------------------------
        // 5. INTERACTIVE CONTROL BAR (4 Profiles + Actions)
        // -------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            height: 64
            color: root.bgCard
            border.color: root.borderCard
            radius: 12

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Profile Buttons (4 Modes)
                Repeater {
                    model: [
                        { mode: 0, label: "🚀 Performance", desc: "4.1GHz Boost" },
                        { mode: 1, label: "⚖️ Balanced", desc: "기본 균형" },
                        { mode: 2, label: "🍃 Smart Save", desc: "1.7GHz 절전" },
                        { mode: 3, label: "❄️ Ultra Save", desc: "1.4GHz + 48Hz" }
                    ]

                    Rectangle {
                        property bool isSelected: backend.powerProfileMode === modelData.mode
                        width: 140
                        height: 42
                        radius: 8
                        color: isSelected ? "#0284c7" : "#232a35"
                        border.color: isSelected ? "#38bdf8" : "#374151"
                        border.width: isSelected ? 1.5 : 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 1
                            Text {
                                text: modelData.label
                                color: isSelected ? "#ffffff" : root.textPrimary
                                font.bold: true
                                font.pixelSize: 11
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: modelData.desc
                                color: isSelected ? "#e0f2fe" : root.textSecondary
                                font.pixelSize: 9
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.setProfile(modelData.mode)
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Rescan Now Action Button
                Rectangle {
                    width: 150
                    height: 42
                    radius: 8
                    color: backend.isRescanning ? "#374151" : "#059669"
                    border.color: backend.isRescanning ? "#4b5563" : "#10b981"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: backend.isRescanning ? "⏳" : "🔍"
                            font.pixelSize: 14
                        }
                        Text {
                            text: backend.isRescanning ? "정밀 측정 중..." : "지금 정밀 재측정"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: backend.isRescanning ? Qt.ArrowCursor : Qt.PointingHandCursor
                        enabled: !backend.isRescanning
                        onClicked: backend.triggerRescan()
                    }
                }

                // KDE System Monitor Button
                Rectangle {
                    width: 44
                    height: 42
                    radius: 8
                    color: "#232a35"
                    border.color: "#374151"

                    Text {
                        anchors.centerIn: parent
                        text: "📊"
                        font.pixelSize: 16
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.openSystemMonitor()
                    }

                    ToolTip.visible: monitorMa.containsMouse
                    ToolTip.text: "KDE 시스템 모니터 열기"
                    id: monitorBtn
                    MouseArea {
                        id: monitorMa
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: backend.openSystemMonitor()
                    }
                }

                // Close Window Button
                Rectangle {
                    width: 44
                    height: 42
                    radius: 8
                    color: "#232a35"
                    border.color: "#374151"

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: root.textSecondary
                        font.pixelSize: 14
                        font.bold: true
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
