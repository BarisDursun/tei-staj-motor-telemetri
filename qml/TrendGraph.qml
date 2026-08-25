import QtQuick 2.15
import QtQuick.Layouts 1.15

// Tek bir parametre icin kayan (scrolling) trend grafigi. Gercek ECTM (Engine
// Condition Trend Monitoring) yazilimlarindaki gibi esik/limit cizgileri gosterilir.
Rectangle {
    id: root

    // --- BİLEŞENİN API'Sİ (Dışarıdan girilecek özellikler) ---
    property string label: ""
    property string unit: ""
    property real value: 0       // Anlık olarak çizilecek değer
    property real minY: 0        // Y ekseninin alt sınırı
    property real maxY: 100      // Y ekseninin üst sınırı

    // Eşik (Alarm) Değerleri (NaN bırakılırsa o çizgi çizilmez)
    property real warnAt: NaN
    property real critAt: NaN
    property real warnLow: NaN
    property real critLow: NaN

    // Tema Renkleri
    property color lineColor: "#4fd1c5"
    property color warnColor: "#ffb300"
    property color critColor: "#ff3b30"
    property color textDim: "#6b7480"
    property color textNormal: "#e8eef2"
    property color labelColor: "#00e5ff"

    property int maxPoints: 90 // Grafikte aynı anda gösterilecek maksimum geçmiş nokta sayısı

    color: "#15171b"; border.color: "#2c3038"; radius: 4

    property var history: []   // Geçmiş verileri tutan dizi
    property int hoverIndex: -1 // Farenin üzerinde olduğu verinin indeksi

    // Eşik değerlerine göre anlık sayının (metnin) rengini belirleyen fonksiyon.
    function statusColorFor(v) {
        if (!isNaN(critAt) && v >= critAt) return critColor
        if (!isNaN(critLow) && v <= critLow) return critColor
        if (!isNaN(warnAt) && v >= warnAt) return warnColor
        if (!isNaN(warnLow) && v <= warnLow) return warnColor
        return textNormal
    }

    // --- ZAMANLAYICI (Veri Toplama Döngüsü) ---
    // Simülasyondan bağımsız, her 400ms'de bir değeri history dizisine ekler.
    Timer {
        interval: 400
        running: true
        repeat: true
        onTriggered: {
            root.history.push(root.value)
            // Eğer dizi kapasitesi dolduysa, en eski (ilk) veriyi silerek kayma efekti yaratır.
            if (root.history.length > root.maxPoints) root.history.shift()
            canvas.requestPaint() // Grafiği yeniden çizmesi için Canvas'ı tetikler.
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 4

        // --- ÜST BİLGİ ÇUBUĞU (Başlık ve Anlık Değer) ---
        RowLayout {
            Layout.fillWidth: true
            Text { text: root.label; color: root.labelColor; font.pixelSize: 11; font.bold: true }
            Item { Layout.fillWidth: true } // Araya boşluk atarak sağa yaslar.
            Text {
                text: root.value.toFixed(1) + " " + root.unit
                color: root.statusColorFor(root.value) // Duruma göre renk değiştirir.
                font.pixelSize: 13; font.bold: true
            }
        }

        // --- GRAFİK ÇİZİM ALANI ---
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Canvas {
                id: canvas
                anchors.fill: parent

                // Fiziksel bir değeri (örn: 500 °C), ekranın piksellerine (Y ekseni) oranlayan matematiksel fonksiyon.
                function yFor(v) {
                    var t = (v - root.minY) / (root.maxY - root.minY)
                    t = Math.max(0, Math.min(1, t))
                    return height - t * height // Çizimi aşağıdan yukarı başlatmak için ters çevirir.
                }

                // Çizimin kalbi: Canvas her tetiklendiğinde burası çalışır.
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height) // Önceki kareyi temizler.

                    // 1. Arka plan referans ızgaraları (%25, %50, %75)
                    ctx.strokeStyle = "#25282e"
                    ctx.lineWidth = 1
                    for (var g = 1; g < 4; g++) {
                        var gy = height * (g / 4)
                        ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke()
                    }

                    // 2. Tehlike Eşik Çizgileri (Kesikli uyarı ve kritik barları)
                    ctx.setLineDash([4, 3]) // Kesikli çizgi efekti.
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
                    // Alt sınır eşikleri (Yağ basıncı vb. için)
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
                    ctx.setLineDash([]) // Çizgi stilini normale döndür.

                    // 3. Ana Trend Çizgisi (Geçmiş verileri noktadan noktaya birleştirir)
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

                    // 4. Etkileşim: Fareyle üzerine gelince (hover) beliren dikey imleç ve nokta.
                    if (root.hoverIndex >= 0 && root.hoverIndex < h.length) {
                        var hx = (root.hoverIndex / (root.maxPoints - 1)) * width
                        var hy = yFor(h[root.hoverIndex])
                        ctx.strokeStyle = "#8a94a3"; ctx.lineWidth = 1
                        ctx.beginPath(); ctx.moveTo(hx, 0); ctx.lineTo(hx, height); ctx.stroke()
                        ctx.beginPath(); ctx.arc(hx, hy, 3, 0, 2 * Math.PI); ctx.fillStyle = root.lineColor; ctx.fill()
                    }

                    // 5. Y Ekseninin minimum ve maksimum sayısal değerlerini köşelere yazar.
                    ctx.fillStyle = root.textDim
                    ctx.font = "9px sans-serif"
                    ctx.fillText(root.maxY.toFixed(0), 3, 10)
                    ctx.fillText(root.minY.toFixed(0), 3, height - 3)
                }

                // Fare ile seçilen indeks değiştiğinde çizimi günceller.
                Connections {
                    target: root
                    function onHoverIndexChanged() { canvas.requestPaint() }
                }
            }

            // --- FARE ETKİLEŞİM ALANI ---
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                // Fare hareket ettikçe, imlecin bulunduğu piksele en yakın veri indeksini (hoverIndex) bulur.
                onPositionChanged: {
                    var idx = Math.round((mouseX / width) * (root.maxPoints - 1))
                    root.hoverIndex = Math.max(0, Math.min(root.history.length - 1, idx))
                }
                onExited: root.hoverIndex = -1 // Fare grafikten çıkınca imleci gizler.
            }

            // --- İPUCU (TOOLTIP) KUTUSU ---
            // Farenin üzerinde olduğu geçmiş veri noktasının değerini ekranda gösteren küçük kutucuk.
            Rectangle {
                visible: root.hoverIndex >= 0 && root.hoverIndex < root.history.length

                // Kutunun X koordinatını farenin konumuna göre ayarlar. Ekrandan taşmaması için Math.min/max kullanılmış.
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