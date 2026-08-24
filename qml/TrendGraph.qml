import QtQuick 2.15
import QtQuick.Layouts 1.15

// Tek bir parametre icin kayan (scrolling) trend grafigi. Gercek ECTM (Engine
// Condition Trend Monitoring) yazilimlarindaki gibi esik/limit cizgileri
// gosterilir - sadece ham veri degil, "tehlikeli bolge" referansi da var.
// Renk kurali: TUM grafiklerde AYNI iz rengi kullanilir (kimlik/marka rengi),
// amber/kirmizi SADECE esik asildiginda kullanilir - durum renkleri suslemeye
// harcanmaz (dataviz iyi pratikleri).
Rectangle {
    id: root
    property string label: ""
    property string unit: ""
    property real value: 0
    property real minY: 0
    property real maxY: 100
    // Esik degerleri opsiyonel - NaN birakilirsa o cizgi hic cizilmez.
    property real warnAt: NaN
    property real critAt: NaN
    property real warnLow: NaN
    property real critLow: NaN
    property color lineColor: "#4fd1c5"
    property color warnColor: "#ffb300"
    property color critColor: "#ff3b30"
    property color textDim: "#6b7480"
    property color textNormal: "#e8eef2"
    property color labelColor: "#00e5ff"
    property int maxPoints: 90

    color: "#15171b"; border.color: "#2c3038"; radius: 4

    property var history: []
    property int hoverIndex: -1

    function statusColorFor(v) {
        if (!isNaN(critAt) && v >= critAt) return critColor
        if (!isNaN(critLow) && v <= critLow) return critColor
        if (!isNaN(warnAt) && v >= warnAt) return warnColor
        if (!isNaN(warnLow) && v <= warnLow) return warnColor
        return textNormal
    }

    // Simulasyon tikinden (200ms) bagimsiz, sabit bir ornekleme hizi -
    // grafik gercek bir trend kaydedici gibi belirli araliklarla ornek alir.
    Timer {
        interval: 400
        running: true
        repeat: true
        onTriggered: {
            root.history.push(root.value)
            if (root.history.length > root.maxPoints) root.history.shift()
            canvas.requestPaint()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Text { text: root.label; color: root.labelColor; font.pixelSize: 11; font.bold: true }
            Item { Layout.fillWidth: true }
            Text {
                text: root.value.toFixed(1) + " " + root.unit
                color: root.statusColorFor(root.value)
                font.pixelSize: 13; font.bold: true
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Canvas {
                id: canvas
                anchors.fill: parent

                function yFor(v) {
                    var t = (v - root.minY) / (root.maxY - root.minY)
                    t = Math.max(0, Math.min(1, t))
                    return height - t * height
                }

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    // Referans izgara (%25/%50/%75) - recessive, sadece goz icin hafif kilavuz.
                    ctx.strokeStyle = "#25282e"
                    ctx.lineWidth = 1
                    for (var g = 1; g < 4; g++) {
                        var gy = height * (g / 4)
                        ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke()
                    }

                    // Esik (limit) cizgileri - gercek ECTM/strip-chart kayitlarindaki
                    // gibi kesikli. Ana Panel'deki AYNI esik degerleriyle besleniyor.
                    ctx.setLineDash([4, 3])
                    ctx.lineWidth = 1
                    if (!isNaN(root.warnAt)) {
                        ctx.strokeStyle = root.warnColor
                        var wy = yFor(root.warnAt)
                        ctx.beginPath(); ctx.moveTo(0, wy); ctx.lineTo(width, wy); ctx.stroke()
                    }
                    if (!isNaN(root.critAt)) {
                        ctx.strokeStyle = root.critColor
                        var cy = yFor(root.critAt)
                        ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(width, cy); ctx.stroke()
                    }
                    if (!isNaN(root.warnLow)) {
                        ctx.strokeStyle = root.warnColor
                        var wly = yFor(root.warnLow)
                        ctx.beginPath(); ctx.moveTo(0, wly); ctx.lineTo(width, wly); ctx.stroke()
                    }
                    if (!isNaN(root.critLow)) {
                        ctx.strokeStyle = root.critColor
                        var cly = yFor(root.critLow)
                        ctx.beginPath(); ctx.moveTo(0, cly); ctx.lineTo(width, cly); ctx.stroke()
                    }
                    ctx.setLineDash([])

                    var h = root.history
                    if (h.length >= 2) {
                        ctx.beginPath()
                        ctx.strokeStyle = root.lineColor
                        ctx.lineWidth = 2
                        for (var i = 0; i < h.length; i++) {
                            var x = (i / (root.maxPoints - 1)) * width
                            var y = yFor(h[i])
                            if (i === 0) ctx.moveTo(x, y)
                            else ctx.lineTo(x, y)
                        }
                        ctx.stroke()
                    }

                    // Fareyle uzerine gelince dikey capraz cizgi + nokta.
                    if (root.hoverIndex >= 0 && root.hoverIndex < h.length) {
                        var hx = (root.hoverIndex / (root.maxPoints - 1)) * width
                        var hy = yFor(h[root.hoverIndex])
                        ctx.strokeStyle = "#8a94a3"; ctx.lineWidth = 1
                        ctx.beginPath(); ctx.moveTo(hx, 0); ctx.lineTo(hx, height); ctx.stroke()
                        ctx.beginPath(); ctx.arc(hx, hy, 3, 0, 2 * Math.PI); ctx.fillStyle = root.lineColor; ctx.fill()
                    }

                    // Olcek uc degerleri - gercek trend kaydedicilerde her zaman gorunur.
                    ctx.fillStyle = root.textDim
                    ctx.font = "9px sans-serif"
                    ctx.fillText(root.maxY.toFixed(0), 3, 10)
                    ctx.fillText(root.minY.toFixed(0), 3, height - 3)
                }

                Connections {
                    target: root
                    function onHoverIndexChanged() { canvas.requestPaint() }
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onPositionChanged: {
                    var idx = Math.round((mouseX / width) * (root.maxPoints - 1))
                    root.hoverIndex = Math.max(0, Math.min(root.history.length - 1, idx))
                }
                onExited: root.hoverIndex = -1
            }

            Rectangle {
                visible: root.hoverIndex >= 0 && root.hoverIndex < root.history.length
                x: Math.min(parent.width - width - 4, Math.max(4, (root.hoverIndex / (root.maxPoints - 1)) * parent.width - width / 2))
                y: 2
                width: tipText.implicitWidth + 10
                height: tipText.implicitHeight + 6
                color: "#0a0b0d"; border.color: "#2c3038"; radius: 3

                Text {
                    id: tipText
                    anchors.centerIn: parent
                    text: root.hoverIndex >= 0 && root.hoverIndex < root.history.length
                        ? root.history[root.hoverIndex].toFixed(1) + " " + root.unit
                        : ""
                    color: root.textNormal; font.pixelSize: 10
                }
            }
        }
    }
}
