import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: reportWin
    // REF-REQ-119: Top-level standalone report window for --report CLI flag
    visible: true
    width: 1180
    height: 780
    minimumWidth: 1000
    minimumHeight: 660
    title: "WattCurb 배터리 심층 드레인 전수 분석 리포트 (Deep Battery Drain Telemetry Audit)"
    color: "#0b0e12"

    // REUSABLE TACTILE CYBER BUTTON COMPONENT (REF-REQ-081, REF-ARCH-058)
    component TactileButton: Button {
        id: tBtn
        property color accentColor: "#00d2ff"
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
        }
    }

    BatteryReportView {
        anchors.fill: parent
    }
}
