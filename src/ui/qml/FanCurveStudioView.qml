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
        // 2. MAIN SPLINE CANVAS & INTERACTIVE WORKSPACE (GPU ACCELERATED)
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
                if (splineCanvas && typeof splineCanvas.requestPaint === "function") {
                    splineCanvas.requestPaint();
                }
            }

            // -----------------------------------------------------------------
            // 2.1 STATIC BACKGROUND GRID CANVAS (Zero Repaint on Drag / Telemetry)
            // -----------------------------------------------------------------
            Canvas {
                id: gridCanvas
                anchors.fill: parent
                renderStrategy: Canvas.Threaded
                renderTarget: Canvas.FramebufferObject
                antialiasing: true

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var w = canvasContainer.plotW;
                    var h = canvasContainer.plotH;
                    var pl = canvasContainer.padLeft;
                    var pr = canvasContainer.padRight;
                    var pt = canvasContainer.padTop;

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
                }
            }

            // -----------------------------------------------------------------
            // 2.2 DYNAMIC SPLINE CURVE CANVAS (Hardware FBO Accelerated)
            // -----------------------------------------------------------------
            Canvas {
                id: splineCanvas
                anchors.fill: parent
                renderStrategy: Canvas.Threaded
                renderTarget: Canvas.FramebufferObject
                antialiasing: true

                Connections {
                    target: backend
                    function onFanCurveDataChanged() {
                        splineCanvas.requestPaint();
                    }
                }

                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var w = canvasContainer.plotW;
                    var h = canvasContainer.plotH;
                    var pl = canvasContainer.padLeft;
                    var pt = canvasContainer.padTop;

                    if (w <= 0 || h <= 0) return;

                    var luts = backend.fanLookupPcts;
                    if (!luts || luts.length < 41) return;

                    var profCol = studioRoot.profileColors[backend.selectedFanProfile] || studioRoot.colCyan;

                    // A. Smooth Area Gradient Fill under curve
                    var grad = ctx.createLinearGradient(0, pt, 0, pt + h);
                    grad.addColorStop(0.0, Qt.rgba(profCol.r, profCol.g, profCol.b, 0.40));
                    grad.addColorStop(0.7, Qt.rgba(profCol.r, profCol.g, profCol.b, 0.12));
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

                    ctx.lineTo(pl + w, pt + h);
                    ctx.closePath();
                    ctx.fillStyle = grad;
                    ctx.fill();

                    // B. Crisp GPU-Accelerated Monotone Curve Stroke (No software blur)
                    ctx.beginPath();
                    ctx.moveTo(startX, startY);
                    for (var ti2 = 1; ti2 < 41; ++ti2) {
                        var cx2 = pl + (ti2 / 40.0) * w;
                        var cy2 = canvasContainer.pctToY(luts[ti2]);
                        ctx.lineTo(cx2, cy2);
                    }
                    ctx.strokeStyle = profCol;
                    ctx.lineWidth = 3.0;
                    ctx.stroke();
                }
            }

            // -----------------------------------------------------------------
            // 2.3 REAL-TIME CROSSHAIR & READOUT (Pure GPU SceneGraph Elements)
            // -----------------------------------------------------------------
            Item {
                id: crosshairItem
                anchors.fill: parent
                visible: backend.cpuTempC >= 25 && backend.cpuTempC <= 85

                readonly property real curTemp: backend.cpuTempC
                readonly property real curRpm: backend.fanRpm
                readonly property real crossX: canvasContainer.tempToX(curTemp)
                readonly property real curPctY: {
                    var luts = backend.fanLookupPcts;
                    if (curTemp >= 30 && curTemp <= 70 && luts && luts.length >= 41) {
                        var idx = Math.min(40, Math.max(0, Math.round(curTemp - 30)));
                        return canvasContainer.pctToY(luts[idx]);
                    } else if (curTemp > 70) {
                        return canvasContainer.padTop;
                    }
                    return canvasContainer.padTop + canvasContainer.plotH;
                }

                // Vertical dashed-effect guideline
                Rectangle {
                    x: crosshairItem.crossX - 0.75
                    y: canvasContainer.padTop
                    width: 1.5
                    height: canvasContainer.plotH
                    color: "#38bdf8"
                    opacity: 0.75
                }

                // Intersection Dot
                Rectangle {
                    x: crosshairItem.crossX - 6
                    y: crosshairItem.curPctY - 6
                    width: 12
                    height: 12
                    radius: 6
                    color: "#38bdf8"
                    border.color: "#ffffff"
                    border.width: 2
                }

                // Readout Badge
                Rectangle {
                    x: Math.min(canvasContainer.padLeft + canvasContainer.plotW - width, Math.max(canvasContainer.padLeft, crosshairItem.crossX - width / 2))
                    y: Math.max(canvasContainer.padTop + 4, crosshairItem.curPctY - 26)
                    width: readoutLabel.implicitWidth + 16
                    height: 22
                    radius: 4
                    color: "#0c4a6e"
                    border.color: "#38bdf8"
                    border.width: 1

                    Text {
                        id: readoutLabel
                        anchors.centerIn: parent
                        text: crosshairItem.curTemp.toFixed(0) + "°C · " + crosshairItem.curRpm + " RPM"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 11
                        font.family: "Monospace"
                    }
                }
            }

            // -----------------------------------------------------------------
            // 2.4 VISUAL CONTROL POINT HANDLES (Pure Visual Presentation)
            // -----------------------------------------------------------------
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
                    width: 32
                    height: 32
                    z: isSelected ? 30 : 15

                    Rectangle {
                        anchors.centerIn: parent
                        width: handleItem.isSelected ? 18 : 14
                        height: handleItem.isSelected ? 18 : 14
                        radius: width / 2
                        color: handleItem.isSelected ? "#ffffff" : studioRoot.profileColors[backend.selectedFanProfile]
                        border.color: handleItem.isSelected ? studioRoot.colCyan : "#0f172a"
                        border.width: 2

                        Behavior on width { NumberAnimation { duration: 80 } }
                        Behavior on height { NumberAnimation { duration: 80 } }

                        // Outer focus ring
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 10
                            height: parent.height + 10
                            radius: width / 2
                            color: "transparent"
                            border.color: studioRoot.profileColors[backend.selectedFanProfile]
                            border.width: 2
                            opacity: handleItem.isSelected ? 0.9 : 0.0
                        }
                    }

                    // Hover/Drag tooltip label
                    Rectangle {
                        visible: handleItem.isSelected || (curveInteractionArea.hoveredIndex === index && !studioRoot.isDragging)
                        anchors.bottom: parent.top
                        anchors.bottomMargin: 4
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
                }
            }

            // -----------------------------------------------------------------
            // 2.5 UNIFIED INTERACTION OVERLAY (Zero-Jank Nearest-Point Drag Engine)
            // -----------------------------------------------------------------
            MouseArea {
                id: curveInteractionArea
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                z: 50
                acceptedButtons: Qt.LeftButton

                property int hoveredIndex: -1
                readonly property real hitRadius: 28

                function findNearestPointIndex(mx, my) {
                    var pts = backend.fanControlPoints;
                    if (!pts || pts.length === 0) return -1;
                    var bestIdx = -1;
                    var bestDistSq = hitRadius * hitRadius;

                    for (var i = 0; i < pts.length; ++i) {
                        var px = canvasContainer.tempToX(pts[i].temp);
                        var py = canvasContainer.pctToY(pts[i].pct);
                        var dx = mx - px;
                        var dy = my - py;
                        var distSq = dx * dx + dy * dy;
                        if (distSq <= bestDistSq) {
                            bestDistSq = distSq;
                            bestIdx = i;
                        }
                    }
                    return bestIdx;
                }

                onPositionChanged: function(mouse) {
                    if (pressed && studioRoot.isDragging && studioRoot.activePointIndex >= 0) {
                        // Directly map canvasContainer mouse coordinates to temperature and speed percentage
                        var newTemp = canvasContainer.xToTemp(mouse.x);
                        var newPct = canvasContainer.yToPct(mouse.y);
                        backend.setFanPoint(studioRoot.activePointIndex, newTemp, newPct);
                    } else {
                        // Hover detection for smooth cursor feedback
                        var hit = findNearestPointIndex(mouse.x, mouse.y);
                        hoveredIndex = hit;
                        cursorShape = (hit >= 0) ? Qt.PointingHandCursor : Qt.ArrowCursor;
                    }
                }

                onPressed: function(mouse) {
                    var hit = findNearestPointIndex(mouse.x, mouse.y);
                    if (hit >= 0) {
                        studioRoot.activePointIndex = hit;
                        studioRoot.isDragging = true;
                        cursorShape = Qt.ClosedHandCursor;
                    } else {
                        // Clicked empty area: deselect
                        studioRoot.activePointIndex = -1;
                        studioRoot.isDragging = false;
                    }
                }

                onReleased: function(mouse) {
                    studioRoot.isDragging = false;
                    var hit = findNearestPointIndex(mouse.x, mouse.y);
                    hoveredIndex = hit;
                    cursorShape = (hit >= 0) ? Qt.PointingHandCursor : Qt.ArrowCursor;
                }

                onCanceled: {
                    studioRoot.isDragging = false;
                    cursorShape = Qt.ArrowCursor;
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
