import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: studioRoot
    clip: true

    readonly property color bgPanel: "#12161d"
    readonly property color bgPanelHeader: "#181e26"
    readonly property color borderPanel: "#222a36"
    readonly property color textMain: "#e5e7eb"
    readonly property color textDim: "#9ca3af"
    readonly property color textMuted: "#6b7280"

    readonly property color colCyan: "#00d2ff"
    readonly property color colGreen: "#10b981"
    readonly property color colOrange: "#f59e0b"
    readonly property color colRed: "#ef4444"
    readonly property color colPurple: "#a855f7"
    readonly property color colBlue: "#3b82f6"

    property int activePointIndex: -1
    property bool isDragging: false
    property string statusToast: ""

    Timer {
        id: statusToastTimer
        interval: 3000
        onTriggered: studioRoot.statusToast = ""
    }

    // Profile names and colors
    readonly property var profileNames: ["Performance", "Balanced", "PowerSaver", "UltraEndurance"]
    readonly property var profileIcons: ["🚀", "⚖️", "🍃", "❄️"]
    readonly property var profileColors: [colRed, colCyan, colGreen, colBlue]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // =====================================================================
        // 1. TOP CONTROL BAR (Profile Selector & Curve State)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: studioRoot.bgPanel
            border.color: studioRoot.borderPanel
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                // Title & Icon
                RowLayout {
                    spacing: 8
                    Rectangle {
                        width: 32; height: 32; radius: 6; color: "#0c4a6e"
                        Text { anchors.centerIn: parent; text: "🌀"; font.pixelSize: 18 }
                    }
                    ColumnLayout {
                        spacing: 1
                        Text {
                            text: "FAN CURVE STUDIO (SIMD VECTORIZED)"
                            color: studioRoot.colCyan
                            font.bold: true
                            font.pixelSize: 15
                            font.family: "Monospace"
                        }
                        Text {
                            text: "ThinkPad EC Monotone Hermite Spline · 30°C~70°C (최대 10포인트 · 70°C 풀스피드 안전 잠금)"
                            color: studioRoot.textDim
                            font.pixelSize: 11
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Profile Selector Pills
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: 4
                        delegate: Rectangle {
                            id: profPill
                            height: 34
                            implicitWidth: pillText.implicitWidth + 24
                            radius: 6
                            color: backend.selectedFanProfile === index ? 
                                Qt.rgba(studioRoot.profileColors[index].r, studioRoot.profileColors[index].g, studioRoot.profileColors[index].b, 0.25) : 
                                "#18202c"
                            border.color: backend.selectedFanProfile === index ? studioRoot.profileColors[index] : "#2d3a4e"
                            border.width: backend.selectedFanProfile === index ? 2 : 1

                            RowLayout {
                                id: pillLayout
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: studioRoot.profileIcons[index]
                                    font.pixelSize: 13
                                }
                                Text {
                                    id: pillText
                                    text: studioRoot.profileNames[index]
                                    color: backend.selectedFanProfile === index ? "#ffffff" : studioRoot.textDim
                                    font.bold: backend.selectedFanProfile === index
                                    font.pixelSize: 12
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    backend.selectFanProfile(index);
                                    studioRoot.activePointIndex = -1;
                                    canvasContainer.requestPaint();
                                }
                            }
                        }
                    }
                }

                Rectangle { width: 1; height: 26; color: studioRoot.borderPanel }

                // Status Badge
                Rectangle {
                    height: 28
                    implicitWidth: statusText.implicitWidth + 16
                    radius: 4
                    color: backend.isCustomFanCurve ? "#3b1e0a" : "#0d2e24"
                    border.color: backend.isCustomFanCurve ? studioRoot.colOrange : studioRoot.colGreen
                    border.width: 1
                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        text: backend.isCustomFanCurve ? "⚙️ 커스텀 커브 활성" : "🔒 기본 팩토리 커브"
                        color: backend.isCustomFanCurve ? studioRoot.colOrange : studioRoot.colGreen
                        font.bold: true
                        font.pixelSize: 11
                    }
                }
            }
        }

        // =====================================================================
        // 2. MAIN SPLINE CANVAS & INTERACTIVE WORKSPACE
        // =====================================================================
        Rectangle {
            id: canvasContainer
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: studioRoot.bgPanel
            border.color: studioRoot.borderPanel
            radius: 8
            clip: true

            readonly property real padLeft: 60
            readonly property real padRight: 70
            readonly property real padTop: 40
            readonly property real padBottom: 50

            readonly property real plotW: width - padLeft - padRight
            readonly property real plotH: height - padTop - padBottom

            function tempToX(t) {
                var clamped = Math.max(30.0, Math.min(70.0, t));
                return padLeft + ((clamped - 30.0) / 40.0) * plotW;
            }

            function xToTemp(x) {
                var ratio = (x - padLeft) / plotW;
                var clampedRatio = Math.max(0.0, Math.min(1.0, ratio));
                return 30.0 + clampedRatio * 40.0;
            }

            function pctToY(p) {
                var clamped = Math.max(0.0, Math.min(100.0, p));
                return padTop + (1.0 - (clamped / 100.0)) * plotH;
            }

            function yToPct(y) {
                var ratio = (y - padTop) / plotH;
                var clampedRatio = Math.max(0.0, Math.min(1.0, ratio));
                return (1.0 - clampedRatio) * 100.0;
            }

            function requestPaint() {
                if (curveCanvas && typeof curveCanvas.requestPaint === "function") {
                    curveCanvas.requestPaint();
                }
            }

            // Canvas for rendering Grid, Monotone Spline Curve, and Failsafe zone
            Canvas {
                id: curveCanvas
                anchors.fill: parent
                antialiasing: true

                Connections {
                    target: backend
                    function onFanCurveDataChanged() {
                        curveCanvas.requestPaint();
                    }
                    function onTelemetryChanged() {
                        curveCanvas.requestPaint();
                    }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var w = canvasContainer.plotW;
                    var h = canvasContainer.plotH;
                    var pl = canvasContainer.padLeft;
                    var pr = canvasContainer.padRight;
                    var pt = canvasContainer.padTop;
                    var pb = canvasContainer.padBottom;

                    if (w <= 0 || h <= 0) return;

                    // 1. Grid Background Fill
                    ctx.fillStyle = "#0d1117";
                    ctx.fillRect(pl, pt, w, h);

                    // 2. Failsafe Zone Pattern (>= 70°C area on right pad)
                    var x70 = pl + w;
                    ctx.fillStyle = "rgba(239, 68, 68, 0.12)";
                    ctx.fillRect(x70, pt, pr, h);
                    ctx.strokeStyle = "rgba(239, 68, 68, 0.4)";
                    ctx.lineWidth = 1;
                    ctx.beginPath();
                    for (var hx = x70; hx < x70 + pr; hx += 10) {
                        ctx.moveTo(hx, pt);
                        ctx.lineTo(hx + 15, pt + h);
                    }
                    ctx.stroke();

                    // Failsafe label
                    ctx.fillStyle = "#ef4444";
                    ctx.font = "bold 11px Monospace";
                    ctx.fillText("FAILSAFE 100%", x70 + 6, pt + 20);
                    ctx.font = "10px Monospace";
                    ctx.fillText("≥ 70°C FULL", x70 + 6, pt + 34);

                    // 3. Horizontal Grid Lines (0%, 20%, 40%, 60%, 80%, 100%)
                    ctx.strokeStyle = "#1b2330";
                    ctx.lineWidth = 1;
                    ctx.font = "11px Monospace";
                    ctx.fillStyle = "#64748b";

                    var tpLevels = ["0", "1", "2", "3", "5", "FULL"];
                    var pcts = [0, 20, 40, 60, 80, 100];
                    for (var i = 0; i < pcts.length; ++i) {
                        var gy = pt + (1.0 - pcts[i] / 100.0) * h;
                        ctx.beginPath();
                        ctx.moveTo(pl, gy);
                        ctx.lineTo(pl + w, gy);
                        ctx.stroke();

                        ctx.textAlign = "right";
                        ctx.fillText(pcts[i] + "%", pl - 8, gy + 4);

                        // ThinkPad Level on Right
                        ctx.textAlign = "left";
                        ctx.fillStyle = (i === pcts.length - 1) ? "#ef4444" : "#94a3b8";
                        ctx.fillText("L" + tpLevels[i], pl + w + 6, gy + 4);
                        ctx.fillStyle = "#64748b";
                    }

                    // 4. Vertical Grid Lines (30°C to 70°C in steps of 5°C)
                    ctx.textAlign = "center";
                    for (var t = 30; t <= 70; t += 5) {
                        var gx = pl + ((t - 30.0) / 40.0) * w;
                        ctx.beginPath();
                        ctx.strokeStyle = (t === 30 || t === 70) ? "#334155" : "#1b2330";
                        ctx.moveTo(gx, pt);
                        ctx.lineTo(gx, pt + h);
                        ctx.stroke();

                        // Label
                        ctx.fillStyle = (t === 70) ? "#ef4444" : ((t === 30) ? "#38bdf8" : "#94a3b8");
                        ctx.font = (t === 30 || t === 70) ? "bold 11px Monospace" : "11px Monospace";
                        ctx.fillText(t + "°C", gx, pt + h + 18);
                    }

                    // Axis Titles
                    ctx.fillStyle = "#94a3b8";
                    ctx.font = "bold 12px Monospace";
                    ctx.textAlign = "center";
                    ctx.fillText("CPU Package Temperature (°C)", pl + w / 2, pt + h + 38);

                    ctx.save();
                    ctx.translate(16, pt + h / 2);
                    ctx.rotate(-Math.PI / 2);
                    ctx.textAlign = "center";
                    ctx.fillText("Fan Target Speed (%)", 0, 0);
                    ctx.restore();

                    // 5. Draw Monotone Spline Curve from SIMD 41-element LUT
                    var luts = backend.fanLookupPcts;
                    if (luts && luts.length >= 41) {
                        // A. Gradient Fill under curve
                        var grad = ctx.createLinearGradient(0, pt, 0, pt + h);
                        var profCol = studioRoot.profileColors[backend.selectedFanProfile] || studioRoot.colCyan;
                        grad.addColorStop(0.0, Qt.rgba(profCol.r, profCol.g, profCol.b, 0.45));
                        grad.addColorStop(0.7, Qt.rgba(profCol.r, profCol.g, profCol.b, 0.15));
                        grad.addColorStop(1.0, Qt.rgba(profCol.r, profCol.g, profCol.b, 0.01));

                        ctx.beginPath();
                        var startX = pl;
                        var startY = canvasContainer.pctToY(luts[0]);
                        ctx.moveTo(startX, pt + h);
                        ctx.lineTo(startX, startY);

                        for (var ti = 1; ti < 41; ++ti) {
                            var cx = pl + (ti / 40.0) * w;
                            var cy = canvasContainer.pctToY(luts[ti]);
                            ctx.lineTo(cx, cy);
                        }

                        // Close path for fill
                        ctx.lineTo(pl + w, pt + h);
                        ctx.closePath();
                        ctx.fillStyle = grad;
                        ctx.fill();

                        // B. Glow stroke line
                        ctx.save();
                        ctx.shadowColor = profCol;
                        ctx.shadowBlur = 10;
                        ctx.strokeStyle = profCol;
                        ctx.lineWidth = 3.0;

                        ctx.beginPath();
                        ctx.moveTo(startX, startY);
                        for (var ti2 = 1; ti2 < 41; ++ti2) {
                            var cx2 = pl + (ti2 / 40.0) * w;
                            var cy2 = canvasContainer.pctToY(luts[ti2]);
                            ctx.lineTo(cx2, cy2);
                        }
                        ctx.stroke();
                        ctx.restore();
                    }

                    // 6. Draw Live Crosshair (Current CPU Temperature & RPM)
                    var curTemp = backend.cpuTempC;
                    var curRpm = backend.fanRpm;
                    if (curTemp >= 25 && curTemp <= 85) {
                        var crossX = canvasContainer.tempToX(curTemp);
                        var curPctY = pt + h; // default bottom
                        if (curTemp >= 30 && curTemp <= 70 && luts && luts.length >= 41) {
                            var idx = Math.min(40, Math.max(0, Math.round(curTemp - 30)));
                            curPctY = canvasContainer.pctToY(luts[idx]);
                        } else if (curTemp > 70) {
                            curPctY = pt; // 100%
                        }

                        // Vertical guideline
                        ctx.save();
                        ctx.strokeStyle = "#38bdf8";
                        ctx.lineWidth = 1.5;
                        ctx.setLineDash([4, 4]);
                        ctx.beginPath();
                        ctx.moveTo(crossX, pt);
                        ctx.lineTo(crossX, pt + h);
                        ctx.stroke();
                        ctx.setLineDash([]);

                        // Active intersection point
                        ctx.fillStyle = "#38bdf8";
                        ctx.shadowColor = "#38bdf8";
                        ctx.shadowBlur = 8;
                        ctx.beginPath();
                        ctx.arc(crossX, curPctY, 6, 0, Math.PI * 2);
                        ctx.fill();

                        // Live readout tooltip badge
                        ctx.fillStyle = "#0c4a6e";
                        ctx.strokeStyle = "#38bdf8";
                        ctx.lineWidth = 1;
                        var badgeW = 100;
                        var badgeH = 22;
                        var badgeX = Math.min(pl + w - badgeW, Math.max(pl, crossX - badgeW / 2));
                        var badgeY = Math.max(pt + 6, curPctY - 30);
                        ctx.fillRect(badgeX, badgeY, badgeW, badgeH);
                        ctx.strokeRect(badgeX, badgeY, badgeW, badgeH);

                        ctx.fillStyle = "#ffffff";
                        ctx.font = "bold 11px Monospace";
                        ctx.textAlign = "center";
                        ctx.fillText(curTemp + "°C · " + curRpm + " RPM", badgeX + badgeW / 2, badgeY + 15);
                        ctx.restore();
                    }
                }
            }

            // Interactive Drag Handles for Control Points
            Repeater {
                id: handleRepeater
                model: backend.fanControlPoints

                delegate: Item {
                    id: handleItem
                    property real ptTemp: modelData.temp
                    property real ptPct: modelData.pct
                    property bool isSelected: studioRoot.activePointIndex === index

                    x: canvasContainer.tempToX(ptTemp) - width / 2
                    y: canvasContainer.pctToY(ptPct) - height / 2
                    width: 24
                    height: 24
                    z: isSelected ? 20 : 10

                    Rectangle {
                        anchors.centerIn: parent
                        width: handleItem.isSelected ? 18 : 14
                        height: handleItem.isSelected ? 18 : 14
                        radius: width / 2
                        color: handleItem.isSelected ? "#ffffff" : studioRoot.profileColors[backend.selectedFanProfile]
                        border.color: handleItem.isSelected ? studioRoot.colCyan : "#0f172a"
                        border.width: 2

                        Behavior on width { NumberAnimation { duration: 100 } }
                        Behavior on height { NumberAnimation { duration: 100 } }

                        // Outer ring glow
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 8
                            height: parent.height + 8
                            radius: width / 2
                            color: "transparent"
                            border.color: studioRoot.profileColors[backend.selectedFanProfile]
                            border.width: 1.5
                            opacity: handleItem.isSelected ? 0.8 : 0.0
                        }
                    }

                    // Hover/Drag readout label
                    Rectangle {
                        visible: handleItem.isSelected || dragMa.containsMouse
                        anchors.bottom: parent.top
                        anchors.bottomMargin: 6
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: handleLabel.implicitWidth + 12
                        height: 20
                        radius: 4
                        color: "#1e293b"
                        border.color: studioRoot.colCyan
                        border.width: 1

                        Text {
                            id: handleLabel
                            anchors.centerIn: parent
                            text: handleItem.ptTemp.toFixed(0) + "°C, " + handleItem.ptPct.toFixed(0) + "%"
                            color: "#ffffff"
                            font.bold: true
                            font.pixelSize: 10
                            font.family: "Monospace"
                        }
                    }

                    MouseArea {
                        id: dragMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        drag.target: parent
                        drag.axis: Drag.XAndYAxis
                        drag.minimumX: canvasContainer.padLeft - parent.width / 2
                        drag.maximumX: canvasContainer.padLeft + canvasContainer.plotW - parent.width / 2
                        drag.minimumY: canvasContainer.padTop - parent.height / 2
                        drag.maximumY: canvasContainer.padTop + canvasContainer.plotH - parent.height / 2

                        onPressed: {
                            studioRoot.activePointIndex = index;
                            studioRoot.isDragging = true;
                        }

                        onPositionChanged: {
                            if (studioRoot.isDragging && pressed) {
                                var newX = parent.x + parent.width / 2;
                                var newY = parent.y + parent.height / 2;
                                var newTemp = canvasContainer.xToTemp(newX);
                                var newPct = canvasContainer.yToPct(newY);

                                backend.setFanPoint(index, newTemp, newPct);
                                canvasContainer.requestPaint();
                            }
                        }

                        onReleased: {
                            studioRoot.isDragging = false;
                            canvasContainer.requestPaint();
                        }
                    }
                }
            }

            // Click canvas to deselect or double-click to add point
            MouseArea {
                anchors.fill: parent
                z: 1
                acceptedButtons: Qt.LeftButton
                onClicked: function(mouse) {
                    if (mouse.x >= canvasContainer.padLeft && mouse.x <= canvasContainer.padLeft + canvasContainer.plotW &&
                        mouse.y >= canvasContainer.padTop && mouse.y <= canvasContainer.padTop + canvasContainer.plotH) {
                        studioRoot.activePointIndex = -1;
                    }
                }
                onDoubleClicked: function(mouse) {
                    if (mouse.x >= canvasContainer.padLeft && mouse.x <= canvasContainer.padLeft + canvasContainer.plotW &&
                        mouse.y >= canvasContainer.padTop && mouse.y <= canvasContainer.padTop + canvasContainer.plotH) {
                        var addT = canvasContainer.xToTemp(mouse.x);
                        var addP = canvasContainer.yToPct(mouse.y);
                        if (backend.fanPointCount < 10) {
                            backend.addFanPoint(addT, addP);
                            studioRoot.statusToast = "포인트 추가됨: " + addT.toFixed(0) + "°C, " + addP.toFixed(0) + "%";
                            statusToastTimer.restart();
                            canvasContainer.requestPaint();
                        } else {
                            studioRoot.statusToast = "최대 10개의 포인트까지만 지원됩니다.";
                            statusToastTimer.restart();
                        }
                    }
                }
            }
        }

        // =====================================================================
        // 3. BOTTOM ACTION BAR & 41-POINT LUT SAMPLER
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: studioRoot.bgPanel
            border.color: studioRoot.borderPanel
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                // Selected Point Quick Editors
                RowLayout {
                    spacing: 8
                    visible: studioRoot.activePointIndex >= 0 && studioRoot.activePointIndex < backend.fanControlPoints.length
                    Text {
                        text: "선택 포인트 [#" + (studioRoot.activePointIndex + 1) + "]:"
                        color: studioRoot.colCyan
                        font.bold: true
                        font.pixelSize: 12
                    }

                    // Delete point button
                    Button {
                        text: "🗑️ 삭제"
                        enabled: backend.fanPointCount > 2
                        onClicked: {
                            backend.removeFanPoint(studioRoot.activePointIndex);
                            studioRoot.activePointIndex = -1;
                            canvasContainer.requestPaint();
                        }
                    }
                }

                // Add Point button when no point selected
                Button {
                    visible: studioRoot.activePointIndex < 0 && backend.fanPointCount < 10
                    text: "➕ 포인트 추가 (더블클릭 또는 클릭)"
                    onClicked: {
                        // Add mid point
                        var curPts = backend.fanControlPoints;
                        var t = 50.0;
                        var p = 50.0;
                        if (curPts.length >= 2) {
                            t = (curPts[0].temp + curPts[curPts.length - 1].temp) / 2.0;
                            p = (curPts[0].pct + curPts[curPts.length - 1].pct) / 2.0;
                        }
                        backend.addFanPoint(t, p);
                        canvasContainer.requestPaint();
                    }
                }

                Item { Layout.fillWidth: true }

                // Toast notification
                Text {
                    text: studioRoot.statusToast
                    color: studioRoot.colOrange
                    font.bold: true
                    font.pixelSize: 12
                }

                // Reset to Default button
                Button {
                    text: "🔄 기본값 복원"
                    onClicked: {
                        backend.resetFanCurveToDefault(backend.selectedFanProfile);
                        studioRoot.statusToast = "프로파일이 팩토리 기본값으로 복원되었습니다.";
                        statusToastTimer.restart();
                        canvasContainer.requestPaint();
                    }
                }

                // Apply Changes button
                Button {
                    id: applyBtn
                    text: "💾 커브 적용 (데몬 반영)"
                    highlighted: true
                    onClicked: {
                        var ok = backend.applyFanCurves();
                        if (ok) {
                            studioRoot.statusToast = "✅ 팬 커브가 성공적으로 저장 및 데몬에 적용되었습니다.";
                        } else {
                            studioRoot.statusToast = "⚠️ 저장 완료 (데몬 상태 확인 필요)";
                        }
                        statusToastTimer.restart();
                        canvasContainer.requestPaint();
                    }
                }
            }
        }
    }
}
