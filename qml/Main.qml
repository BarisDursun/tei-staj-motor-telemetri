import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Tasarim dili EICAS/ECAM (ucak motor gosterge sistemleri) renk kurallarina
// dayaniyor: parametre ISIMLERI cyan, degerler normalken beyaz, esik
// asilinca AMBER (uyari) / KIRMIZI (kritik) olur - degerin kendisi renk
// degistirir, sabit dekoratif renk degil. Yuvarlak/arc gostergeler zaten
// EICAS'ta N1/N2/EGT icin standart yontem (tape/bant degil), o yuzden
// korunuyor. Kaynak: Wikipedia "Electronic centralised aircraft monitor",
// Aeroclass.org "EICAS", Pilot Institute "Round Dials or Glass Cockpits".
ApplicationWindow {
    id: root
    width: 1100
    height: 750
    visible: true
    title: "TEI Motor Telemetri İstasyonu"
    font.family: "Consolas"
    // Varsayilan pencere rengi beyaz - herhangi bir kare gecikmesinde/tam
    // kaplamayan bir alanda beyaz yerine koyu gorunsun diye acikca ayarlandi.
    color: bgDeep

    // --- TEMA (EICAS renk kurallari) ---
    readonly property color bgDeep: "#0a0b0d"
    readonly property color bgPanel: "#15171b"
    readonly property color bgPanelAlt: "#1b1e23"
    readonly property color borderDim: "#2c3038"
    readonly property color cyanC: "#00e5ff"
    readonly property color whiteC: "#e8eef2"
    readonly property color amberC: "#ffb300"
    readonly property color redC: "#ff3b30"
    readonly property color greenC: "#00e676"
    readonly property color dimC: "#6b7480"

    // Esik asilinca deger metni AMBER/KIRMIZI olur (EICAS disiplini) - bant
    // sinirlari engine_core.h Engine::EvaluateAlarm() ile ayni olmali,
    // burada sadece GORSEL amacli tekrarlaniyor (C++ tarafi tek otorite).
    function statusColor(value, warnAt, critAt) {
        if (value >= critAt) return redC
        if (value >= warnAt) return amberC
        return whiteC
    }
    function statusColorRange(value, warnLow, critLow, warnHigh, critHigh) {
        if (value <= critLow || value >= critHigh) return redC
        if (value <= warnLow || value >= warnHigh) return amberC
        return whiteC
    }

    readonly property real thDevir1Warn: selectedEngine === "TF10000" ? 90 : 2500
    readonly property real thDevir1Crit: selectedEngine === "TF10000" ? 97 : 2750
    readonly property real thDevir2Warn: selectedEngine === "TF10000" ? 90 : 95
    readonly property real thDevir2Crit: selectedEngine === "TF10000" ? 97 : 102
    readonly property real thEgtWarn: selectedEngine === "TF10000" ? 775 : 480
    readonly property real thEgtCrit: selectedEngine === "TF10000" ? 850 : 550
    readonly property real thTitresimWarn: selectedEngine === "TF10000" ? 4.0 : 2.2
    readonly property real thTitresimCrit: selectedEngine === "TF10000" ? 5.0 : 2.7
    readonly property real thYagBasWarnLow: selectedEngine === "TF10000" ? 35.0 : 2.0
    readonly property real thYagBasCritLow: selectedEngine === "TF10000" ? 30.0 : 1.5
    readonly property real thYagBasWarnHigh: selectedEngine === "TF10000" ? 75.0 : 5.8
    readonly property real thYagBasCritHigh: selectedEngine === "TF10000" ? 80.0 : 6.0

    // Grafikler sekmesindeki 8 trend grafiginin HEPSI bu tek izi kullanir -
    // amber/kirmizi sadece esik gostergesi icin ayrildi, sus icin degil.
    readonly property color traceC: "#4fd1c5"

    // --- DURUM YÖNETİMİ ---
    property string selectedEngine: ""
    property bool isEngineOn: false
    property real mockPower: isEngineOn ? powerSlider.value : 0
    // Filodaki (10 yasli TF10000) hangi motorun secili oldugunu isaretlemek
    // icin - baslangicta kart ekranindan secilen motor (0 yasinda) fleet id=1
    // ile ayni yasa denk geldigi icin varsayilan olarak 1 isaretli.
    property int selectedFleetId: 1

    // --- C++ BAĞLANTISI ---
    property real p_Devir1: engineModel.p_Devir1
    property real p_Devir2: engineModel.p_Devir2
    property real p_Basinc: engineModel.p_Basinc
    property real p_EGT: engineModel.p_EGT
    property real p_Yakit: engineModel.p_Yakit
    property real p_YagBasinci: engineModel.p_YagBasinci
    property real p_YagSicakligi: engineModel.p_YagSicakligi
    property real p_Titresim: engineModel.p_Titresim

    // Animasyonlu Akışlar
    property real anim_Devir1: 0
    property real anim_Devir2: 0
    property real anim_Basinc: 1.0
    property real anim_EGT: 20
    property real anim_Yakit: 0
    property real anim_YagBasinci: 0
    property real anim_YagSicakligi: 20
    property real anim_Titresim: 0

    Behavior on anim_Devir1 { NumberAnimation { duration: 1000 } }
    Behavior on anim_Devir2 { NumberAnimation { duration: 1000 } }
    Behavior on anim_Basinc { NumberAnimation { duration: 1200 } }
    Behavior on anim_EGT { NumberAnimation { duration: 1500 } }
    Behavior on anim_Yakit { NumberAnimation { duration: 800 } }
    Behavior on anim_YagBasinci { NumberAnimation { duration: 800 } }
    Behavior on anim_YagSicakligi { NumberAnimation { duration: 2000 } }
    Behavior on anim_Titresim { NumberAnimation { duration: 500 } }

    onP_Devir1Changed: anim_Devir1 = p_Devir1
    onP_Devir2Changed: anim_Devir2 = p_Devir2
    onP_BasincChanged: anim_Basinc = p_Basinc
    onP_EGTChanged: anim_EGT = p_EGT
    onP_YakitChanged: anim_Yakit = p_Yakit
    onP_YagBasinciChanged: anim_YagBasinci = p_YagBasinci
    onP_YagSicakligiChanged: anim_YagSicakligi = p_YagSicakligi
    onP_TitresimChanged: anim_Titresim = p_Titresim

    StackLayout {
        id: mainStack
        anchors.fill: parent
        currentIndex: selectedEngine === "" ? 0 : 1

        // ==========================================
        // SAYFA 0: MOTOR SEÇİM EKRANI
        // ==========================================
        Rectangle {
            color: bgDeep
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 40

                Text {
                    text: "TEI TELEMETRİ SİSTEMİ BAŞLATILIYOR\nLütfen Test Edilecek Motoru Seçin"
                    color: whiteC
                    font.pixelSize: 22; font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    spacing: 40
                    Layout.alignment: Qt.AlignHCenter

                    // TF10000 karti: TEI-TF10000 gercekten var olan bir proje (KAAN/TF-X
                    // icin gelistiriliyor, bkz. tei.com.tr) - ama program cok erken
                    // asamada (ilk ates testi 2027 sonu hedefli), o yuzden burada
                    // gosterilecek performans degerlerinin TEMSILI oldugu acikca
                    // belirtiliyor, gercek/resmi TEI verisi diye sunulmuyor.
                    Rectangle {
                        width: 300; height: 260
                        color: bgPanel; border.color: cyanC; border.width: 2; radius: 6
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 8; width: parent.width - 40

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: badgeText1.implicitWidth + 16; Layout.preferredHeight: 20
                                color: "#0a2a2e"; border.color: cyanC; border.width: 1; radius: 3
                                Text { id: badgeText1; anchors.centerIn: parent; text: "🇹🇷 GERÇEK TEİ PROJESİ"; color: cyanC; font.pixelSize: 9; font.bold: true }
                            }

                            Text { text: "TF10000"; color: cyanC; font.pixelSize: 30; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                            Text { text: "Turbofan Motoru"; color: dimC; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                            Text {
                                text: "KAAN (TF-X) için geliştiriliyor — TEİ'nin yerli turbofan programı"
                                color: whiteC; font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                                Layout.fillWidth: true; Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Buradaki değerler temsili demo verisidir, TEİ'nin resmi performans verisi değildir (motor henüz test edilmedi)."
                                color: dimC; font.pixelSize: 9; font.italic: true
                                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                                Layout.fillWidth: true; Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                selectedEngine = "TF10000"
                                engineModel.selectEngine("TF10000")
                            }
                        }
                    }

                    // PD170 karti: gercekten uretilen bir TEI Iha motoru.
                    Rectangle {
                        width: 300; height: 260
                        color: bgPanel; border.color: amberC; border.width: 2; radius: 6
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 8; width: parent.width - 40

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: badgeText2.implicitWidth + 16; Layout.preferredHeight: 20
                                color: "#2e2205"; border.color: amberC; border.width: 1; radius: 3
                                Text { id: badgeText2; anchors.centerIn: parent; text: "🇹🇷 GERÇEK TEİ ÜRÜNÜ"; color: amberC; font.pixelSize: 9; font.bold: true }
                            }

                            Text { text: "PD170"; color: amberC; font.pixelSize: 30; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                            Text { text: "Turbodizel Havacılık Motoru"; color: dimC; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                            Text {
                                text: "TEİ'nin ürettiği, İHA/hafif uçak sınıfı yerli turbodizel motoru"
                                color: whiteC; font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                                Layout.fillWidth: true; Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Buradaki değerler temsili demo verisidir, TEİ'nin resmi performans verisi değildir."
                                color: dimC; font.pixelSize: 9; font.italic: true
                                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                                Layout.fillWidth: true; Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                selectedEngine = "PD170"
                                engineModel.selectEngine("PD170")
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // SAYFA 1: ANA ARAYÜZ (TELEMETRİ)
        // ==========================================
        Item {
            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // --- ÜST BAR ---
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    color: bgPanel
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: borderDim }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        Text {
                            text: "TEI-" + selectedEngine
                            color: cyanC; font.bold: true; font.pixelSize: 13
                            Layout.leftMargin: 14
                        }

                        TabBar {
                            id: tabBar
                            Layout.fillWidth: true
                            Layout.leftMargin: 24
                            background: Rectangle { color: "transparent" }

                            TabButton {
                                text: "ANA PANEL"
                                contentItem: Text {
                                    text: parent.text; color: parent.checked ? cyanC : dimC
                                    font.bold: true; font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: "transparent"
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: parent.parent.checked ? cyanC : "transparent" }
                                }
                            }
                            TabButton {
                                text: "GRAFİKLER"
                                contentItem: Text {
                                    text: parent.text; color: parent.checked ? cyanC : dimC
                                    font.bold: true; font.pixelSize: 12
                                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: "transparent"
                                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: parent.parent.checked ? cyanC : "transparent" }
                                }
                            }
                        }

                        // Motorun genel durumu - EngineModel::alarmLevel'dan tek kaynaktan okunur.
                        Rectangle {
                            Layout.preferredWidth: 110
                            Layout.fillHeight: true
                            color: engineModel.alarmLevelText === "KRİTİK" ? redC
                                 : engineModel.alarmLevelText === "UYARI"  ? amberC
                                 : "#123422"
                            Text {
                                anchors.centerIn: parent
                                text: engineModel.alarmLevelText
                                color: engineModel.alarmLevelText === "NORMAL" ? greenC : "#0a0b0d"
                                font.bold: true
                                font.pixelSize: 13
                            }
                        }

                        Button {
                            text: "MOTOR DEĞİŞTİR"
                            Layout.fillHeight: true
                            Layout.preferredWidth: 150

                            contentItem: Text {
                                text: parent.text; color: whiteC; font.bold: true; font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: "#3a1010"; border.color: redC; border.width: 1 }

                            onClicked: {
                                selectedEngine = ""
                                isEngineOn = false
                                powerSlider.value = 0
                                tabBar.currentIndex = 0
                            }
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabBar.currentIndex

                    // SEKME 1: GÖSTERGELER
                    Rectangle {
                        color: bgDeep
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 16

                            // 1. SOL BLOK: MOTOR START/STOP + FILO SECIMI
                            Rectangle {
                                Layout.preferredWidth: 240
                                Layout.fillHeight: true
                                color: bgPanel; border.color: borderDim; radius: 4
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 16; spacing: 12
                                    Text { text: selectedEngine + " KONTROL"; color: cyanC; font.bold: true; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }

                                    Button {
                                        text: "MOTORU BAŞLAT"
                                        Layout.fillWidth: true; Layout.preferredHeight: 56; enabled: !isEngineOn
                                        onClicked: {
                                            isEngineOn = true
                                            engineModel.setPower(powerSlider.value)
                                            engineModel.startEngine()
                                        }
                                        contentItem: Text { text: parent.text; color: parent.enabled ? "#06140a" : dimC; font.bold: true; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: parent.enabled ? greenC : bgPanelAlt; radius: 3; border.color: borderDim; border.width: 1 }
                                    }
                                    Button {
                                        text: "MOTORU DURDUR"
                                        Layout.fillWidth: true; Layout.preferredHeight: 56; enabled: isEngineOn
                                        onClicked: {
                                            isEngineOn = false
                                            powerSlider.value = 0
                                            engineModel.stopEngine()
                                        }
                                        contentItem: Text { text: parent.text; color: parent.enabled ? "#1a0505" : dimC; font.bold: true; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: parent.enabled ? redC : bgPanelAlt; radius: 3; border.color: borderDim; border.width: 1 }
                                    }

                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: borderDim }

                                    Text { text: "FİLO — TEST İÇİN MOTOR SEÇ"; color: dimC; font.bold: true; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }

                                    // Yaslarina gore farkli asinmis 10 TF10000 - biri secilince
                                    // engineModel.selectFleetEngine(id) o motorun yipranmis
                                    // parametreleriyle simulasyonu sifirdan baslatir.
                                    ListView {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 180
                                        clip: true
                                        spacing: 4
                                        model: engineModel.fleetEngines()

                                        delegate: Rectangle {
                                            width: ListView.view.width
                                            height: 28
                                            radius: 3
                                            color: modelData.id === selectedFleetId ? bgPanelAlt : "transparent"
                                            border.color: modelData.id === selectedFleetId ? cyanC : "transparent"
                                            border.width: 1

                                            Text {
                                                anchors.fill: parent; anchors.margins: 6
                                                verticalAlignment: Text.AlignVCenter
                                                text: modelData.label
                                                color: modelData.id === selectedFleetId ? cyanC : whiteC
                                                font.pixelSize: 11; font.bold: modelData.id === selectedFleetId
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: {
                                                    selectedFleetId = modelData.id
                                                    isEngineOn = false
                                                    powerSlider.value = 0
                                                    engineModel.selectFleetEngine(modelData.id)
                                                }
                                            }
                                        }
                                    }

                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: borderDim }

                                    // Bakim teshisi - kasitli olarak ANA gostergelerden GORSEL OLARAK
                                    // AYRI bir kart: sol kenarda durum rengiyle boyali ince bir serit +
                                    // "TEST SONUCU" basligi, boylece operator canli olcumle tahmine
                                    // dayali teshisi asla karistirmaz (arastirma bulgusu: predictive
                                    // maintenance panellerinde bu ayrimin acik olmasi guven icin sart).
                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: diagCol.implicitHeight + 16
                                        color: bgPanelAlt
                                        radius: 3

                                        readonly property color statusTint: engineModel.maintenanceStatusText === "BAKIM GEREKLİ"       ? redC
                                                                            : engineModel.maintenanceStatusText === "İZLENMELİ"           ? amberC
                                                                            : engineModel.maintenanceStatusText === "HENÜZ TEST EDİLMEDİ" ? dimC
                                                                            : greenC

                                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 3; color: parent.statusTint }

                                        ColumnLayout {
                                            id: diagCol
                                            anchors.fill: parent; anchors.margins: 10; anchors.leftMargin: 14
                                            spacing: 4

                                            Text { text: "🔧 BAKIM TEŞHİSİ — TEST SONUCU"; color: dimC; font.pixelSize: 9; font.bold: true }
                                            Text {
                                                text: engineModel.maintenanceStatusText
                                                color: parent.parent.statusTint
                                                font.bold: true; font.pixelSize: 13
                                            }

                                            // En cok referanstan sapan parametreler, en buyuk sapma basta.
                                            // Motor tamamen saglikliyse (liste bos) hic gorunmez.
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 1
                                                visible: engineModel.wearNotes.length > 0
                                                Repeater {
                                                    model: engineModel.wearNotes
                                                    delegate: Text {
                                                        Layout.fillWidth: true
                                                        text: "› " + modelData
                                                        color: amberC
                                                        font.pixelSize: 9
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 2. ORTA BLOK: 8 PARAMETRE GÖSTERİMİ
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                spacing: 24

                                RowLayout {
                                    Layout.alignment: Qt.AlignHCenter
                                    spacing: 80

                                    // param_Devir1
                                    ColumnLayout {
                                        Canvas {
                                            width: 150; height: 150; Layout.alignment: Qt.AlignHCenter
                                            property real value: anim_Devir1
                                            property real min: 0
                                            // max, motorun EngineModel::GetParamCeilings()'teki gercek tavanina
                                            // esit (TF10000 %100 guctekiyle 100'e, PD170 2800 RPM'e ulasir) -
                                            // aksi halde ibre %100 guçte bile sonuna kadar gitmez.
                                            property real max: selectedEngine === "TF10000" ? 100 : 2800
                                            property real warnStart: thDevir1Warn
                                            property real critStart: thDevir1Crit

                                            onValueChanged: requestPaint()

                                            onPaint: {
                                                var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                                var cx = width / 2; var cy = height / 2; var r = width / 2 - 15
                                                var startAngle = 0.75 * Math.PI; var sweep = 1.5 * Math.PI

                                                // İbrenin tur atmasını engelleyen GÜVENLİK KİLİDİ (Clamp)
                                                function getAngle(v) {
                                                    var clampedV = Math.min(Math.max(v, min), max);
                                                    return startAngle + ((clampedV - min) / (max - min)) * sweep;
                                                }

                                                ctx.lineWidth = 14; ctx.lineCap = "butt"
                                                ctx.beginPath(); ctx.strokeStyle = greenC; ctx.arc(cx, cy, r, getAngle(min), getAngle(warnStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = amberC; ctx.arc(cx, cy, r, getAngle(warnStart), getAngle(critStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = redC; ctx.arc(cx, cy, r, getAngle(critStart), getAngle(max)); ctx.stroke()

                                                // Ibrenin sablonu "yukari" (native 270 derece) cizildigi icin
                                                // rotate'e verilen aci bu dogal yonelimi telafi etmeli, yoksa
                                                // ibre gercek konumundan 270 derece (3/4 tur) kaymis durur.
                                                ctx.save(); ctx.translate(cx, cy); ctx.rotate(getAngle(value) - 1.5 * Math.PI)
                                                ctx.beginPath(); ctx.moveTo(-4, 0); ctx.lineTo(4, 0); ctx.lineTo(0, -(r - 2)); ctx.closePath()
                                                ctx.fillStyle = whiteC; ctx.fill()
                                                ctx.beginPath(); ctx.arc(0, 0, 8, 0, 2*Math.PI); ctx.fillStyle = bgPanel; ctx.fill()
                                                ctx.lineWidth = 2; ctx.strokeStyle = whiteC; ctx.stroke(); ctx.restore()
                                            }
                                        }
                                        Text { text: selectedEngine === "TF10000" ? "N1 FAN" : "MOTOR DEVRİ"; color: cyanC; font.pixelSize: 13; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        Text {
                                            text: anim_Devir1.toFixed(1) + (selectedEngine === "TF10000" ? " %" : " RPM")
                                            color: statusColor(anim_Devir1, thDevir1Warn, thDevir1Crit)
                                            font.pixelSize: 20; font.bold: true; Layout.alignment: Qt.AlignHCenter
                                        }
                                    }

                                    // param_Devir2
                                    ColumnLayout {
                                        Canvas {
                                            width: 150; height: 150; Layout.alignment: Qt.AlignHCenter
                                            property real value: anim_Devir2
                                            property real min: 0
                                            // max, motorun gercek tavanina esit (TF10000 %100'e, PD170'te
                                            // Devir2 aslinda sogutma suyu sicakligi, tavani 105°C).
                                            property real max: selectedEngine === "TF10000" ? 100 : 105
                                            property real warnStart: thDevir2Warn
                                            property real critStart: thDevir2Crit

                                            onValueChanged: requestPaint()

                                            onPaint: {
                                                var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                                var cx = width / 2; var cy = height / 2; var r = width / 2 - 15
                                                var startAngle = 0.75 * Math.PI; var sweep = 1.5 * Math.PI

                                                function getAngle(v) {
                                                    var clampedV = Math.min(Math.max(v, min), max);
                                                    return startAngle + ((clampedV - min) / (max - min)) * sweep;
                                                }

                                                ctx.lineWidth = 14; ctx.lineCap = "butt"
                                                ctx.beginPath(); ctx.strokeStyle = greenC; ctx.arc(cx, cy, r, getAngle(min), getAngle(warnStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = amberC; ctx.arc(cx, cy, r, getAngle(warnStart), getAngle(critStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = redC; ctx.arc(cx, cy, r, getAngle(critStart), getAngle(max)); ctx.stroke()

                                                // Ibrenin sablonu "yukari" (native 270 derece) cizildigi icin
                                                // rotate'e verilen aci bu dogal yonelimi telafi etmeli, yoksa
                                                // ibre gercek konumundan 270 derece (3/4 tur) kaymis durur.
                                                ctx.save(); ctx.translate(cx, cy); ctx.rotate(getAngle(value) - 1.5 * Math.PI)
                                                ctx.beginPath(); ctx.moveTo(-4, 0); ctx.lineTo(4, 0); ctx.lineTo(0, -(r - 2)); ctx.closePath()
                                                ctx.fillStyle = whiteC; ctx.fill()
                                                ctx.beginPath(); ctx.arc(0, 0, 8, 0, 2*Math.PI); ctx.fillStyle = bgPanel; ctx.fill()
                                                ctx.lineWidth = 2; ctx.strokeStyle = whiteC; ctx.stroke(); ctx.restore()
                                            }
                                        }
                                        Text { text: selectedEngine === "TF10000" ? "N2 CORE" : "SOĞUTMA SUYU"; color: cyanC; font.pixelSize: 13; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        Text {
                                            text: anim_Devir2.toFixed(1) + (selectedEngine === "TF10000" ? " %" : " °C")
                                            color: statusColor(anim_Devir2, thDevir2Warn, thDevir2Crit)
                                            font.pixelSize: 20; font.bold: true; Layout.alignment: Qt.AlignHCenter
                                        }
                                    }
                                }

                                GridLayout {
                                    columns: 3
                                    rowSpacing: 16; columnSpacing: 16
                                    Layout.alignment: Qt.AlignHCenter

                                    // Kompresör Basıncı
                                    Rectangle {
                                        Layout.preferredWidth: 150; Layout.preferredHeight: 74
                                        color: bgPanel; border.color: borderDim; radius: 3
                                        ColumnLayout {
                                            anchors.centerIn: parent; spacing: 2
                                            Text { text: selectedEngine === "TF10000" ? "KOMPRESÖR BASINCI" : "MANİFOLD MAP"; color: cyanC; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                            Text { text: anim_Basinc.toFixed(2) + " Bar"; color: whiteC; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        }
                                    }

                                    // EGT
                                    Rectangle {
                                        Layout.preferredWidth: 150; Layout.preferredHeight: 74
                                        color: bgPanel; border.color: borderDim; radius: 3
                                        ColumnLayout {
                                            anchors.centerIn: parent; spacing: 2
                                            Text { text: "EGT (EGZOZ SCK.)"; color: cyanC; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                            Text {
                                                text: anim_EGT.toFixed(0) + " °C"
                                                color: statusColor(anim_EGT, thEgtWarn, thEgtCrit)
                                                font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }

                                    // Yakıt Akışı
                                    Rectangle {
                                        Layout.preferredWidth: 150; Layout.preferredHeight: 74
                                        color: bgPanel; border.color: borderDim; radius: 3
                                        ColumnLayout {
                                            anchors.centerIn: parent; spacing: 2
                                            Text { text: "YAKIT AKIŞI"; color: cyanC; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                            Text { text: anim_Yakit.toFixed(1) + " kg/h"; color: whiteC; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        }
                                    }

                                    // Yağ Basıncı (Taşma Hatası Çözüldü)
                                    Rectangle {
                                        Layout.preferredWidth: 150; Layout.preferredHeight: 74
                                        color: bgPanel; border.color: borderDim; radius: 3
                                        ColumnLayout {
                                            anchors.centerIn: parent; spacing: 2
                                            Text { text: "YAĞ BASINCI"; color: cyanC; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                            RowLayout {
                                                Layout.alignment: Qt.AlignHCenter
                                                spacing: 8
                                                Rectangle {
                                                    width: 14; height: 32; color: "#111"; border.color: borderDim; radius: 2

                                                    Rectangle {
                                                        width: parent.width; anchors.bottom: parent.bottom

                                                        // Seçili motora göre maksimum basınç ayarı
                                                        property real maxPress: selectedEngine === "TF10000" ? 100.0 : 8.0

                                                        // Sütunun kaptan taşmasını engelleyen Math.min kilidi
                                                        height: Math.min((anim_YagBasinci / maxPress) * parent.height, parent.height)

                                                        color: statusColorRange(anim_YagBasinci, thYagBasWarnLow, thYagBasCritLow, thYagBasWarnHigh, thYagBasCritHigh)
                                                        radius: 2
                                                    }
                                                }
                                                Text {
                                                    text: anim_YagBasinci.toFixed(1) + (selectedEngine === "TF10000" ? " psi" : " bar")
                                                    color: statusColorRange(anim_YagBasinci, thYagBasWarnLow, thYagBasCritLow, thYagBasWarnHigh, thYagBasCritHigh)
                                                    font.pixelSize: 20; font.bold: true
                                                }
                                            }
                                        }
                                    }

                                    // Yağ Sıcaklığı
                                    Rectangle {
                                        Layout.preferredWidth: 150; Layout.preferredHeight: 74
                                        color: bgPanel; border.color: borderDim; radius: 3
                                        ColumnLayout {
                                            anchors.centerIn: parent; spacing: 2
                                            Text { text: "YAĞ SICAKLIĞI"; color: cyanC; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                            Text { text: anim_YagSicakligi.toFixed(1) + " °C"; color: whiteC; font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        }
                                    }

                                    // Titreşim
                                    Rectangle {
                                        Layout.preferredWidth: 150; Layout.preferredHeight: 74
                                        color: bgPanel; border.color: borderDim; radius: 3
                                        ColumnLayout {
                                            anchors.centerIn: parent; spacing: 2
                                            Text { text: "TİTREŞİM"; color: cyanC; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                                            Text {
                                                text: anim_Titresim.toFixed(2) + " IPS"
                                                color: statusColor(anim_Titresim, thTitresimWarn, thTitresimCrit)
                                                font.pixelSize: 22; font.bold: true; Layout.alignment: Qt.AlignHCenter
                                            }
                                        }
                                    }
                                }
                            }

                            // 3. SAĞ BLOK: GÜÇ / THROTTLE SLIDER
                            Rectangle {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true
                                color: bgPanel; border.color: borderDim; radius: 4

                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 20; spacing: 15
                                    Text { text: "THRUST"; color: cyanC; font.bold: true; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                    Text { text: "%" + powerSlider.value.toFixed(0); color: whiteC; font.bold: true; font.pixelSize: 22; Layout.alignment: Qt.AlignHCenter }

                                    Slider {
                                        id: powerSlider
                                        orientation: Qt.Vertical; from: 0; to: 100; value: 0
                                        enabled: isEngineOn
                                        Layout.fillHeight: true; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 60

                                        onValueChanged: {
                                            if (isEngineOn) {
                                                engineModel.setPower(value)
                                            }
                                        }

                                        background: Rectangle {
                                            x: powerSlider.leftPadding + (powerSlider.availableWidth - width) / 2
                                            y: powerSlider.topPadding
                                            width: 50; height: powerSlider.availableHeight
                                            color: "#0d0e10"; border.color: borderDim; border.width: 2; radius: 3

                                            Column {
                                                anchors.fill: parent; anchors.margins: 4; spacing: 3
                                                Repeater {
                                                    model: 10
                                                    Rectangle {
                                                        id: segmentBg
                                                        width: parent.width; height: (parent.height - 27) / 10
                                                        color: "#1a1c20"

                                                        // Her segment 10'luk bir deger araligini temsil eder (orn.
                                                        // index=5 -> 40-50). Kolun tam pozisyonuyla piksel bazinda
                                                        // hizali kalsin diye sinirdaki segment KISMI dolduruluyor -
                                                        // eskiden 10'ar 10'ar "ziplayan" bloklu mantik yerine.
                                                        readonly property real segmentTop: (10 - index) * 10
                                                        readonly property real segmentBottom: segmentTop - 10
                                                        readonly property real fillFraction: Math.max(0, Math.min(1, (powerSlider.value - segmentBottom) / 10))

                                                        Rectangle {
                                                            anchors.bottom: parent.bottom
                                                            width: parent.width
                                                            height: parent.height * segmentBg.fillFraction
                                                            color: index < 3 ? amberC : greenC
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        handle: Rectangle {
                                            x: powerSlider.leftPadding + (powerSlider.availableWidth - width) / 2
                                            y: powerSlider.topPadding + powerSlider.visualPosition * (powerSlider.availableHeight - height)
                                            width: 70; height: 14; color: powerSlider.enabled ? whiteC : dimC; border.color: bgDeep; border.width: 2; radius: 2
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // SEKME 2: GRAFİKLER - 8 parametrenin tamami icin kayan trend grafigi.
                    // min/max, motorun gercek tavan degerlerine (bkz. engine_core.h
                    // ParamCeilings) biraz pay birakilarak ayarlandi - asinmis eski
                    // motorlarda deger tavani asabildigi icin (orn. Titresim +%150).
                    Rectangle {
                        color: bgDeep
                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 16

                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: selectedEngine === "TF10000" ? "N1 Fan" : "Motor Devri"
                                unit: selectedEngine === "TF10000" ? "%" : "RPM"
                                value: p_Devir1
                                maxY: selectedEngine === "TF10000" ? 100 : 3000
                                warnAt: thDevir1Warn; critAt: thDevir1Crit
                                lineColor: traceC; labelColor: cyanC; warnColor: amberC; critColor: redC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: selectedEngine === "TF10000" ? "N2 Core" : "Soğutma Suyu"
                                unit: selectedEngine === "TF10000" ? "%" : "°C"
                                value: p_Devir2
                                maxY: selectedEngine === "TF10000" ? 100 : 120
                                warnAt: thDevir2Warn; critAt: thDevir2Crit
                                lineColor: traceC; labelColor: cyanC; warnColor: amberC; critColor: redC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: selectedEngine === "TF10000" ? "Kompresör Basıncı" : "Manifold MAP"
                                unit: "Bar"
                                value: p_Basinc
                                maxY: selectedEngine === "TF10000" ? 20 : 3
                                lineColor: traceC; labelColor: cyanC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: "EGT (Egzoz Sck.)"
                                unit: "°C"
                                value: p_EGT
                                maxY: selectedEngine === "TF10000" ? 1200 : 700
                                warnAt: thEgtWarn; critAt: thEgtCrit
                                lineColor: traceC; labelColor: cyanC; warnColor: amberC; critColor: redC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: "Yakıt Akışı"
                                unit: "kg/h"
                                value: p_Yakit
                                maxY: selectedEngine === "TF10000" ? 3600 : 40
                                lineColor: traceC; labelColor: cyanC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: "Yağ Basıncı"
                                unit: selectedEngine === "TF10000" ? "psi" : "bar"
                                value: p_YagBasinci
                                maxY: selectedEngine === "TF10000" ? 80 : 6
                                warnAt: thYagBasWarnHigh; critAt: thYagBasCritHigh
                                warnLow: thYagBasWarnLow; critLow: thYagBasCritLow
                                lineColor: traceC; labelColor: cyanC; warnColor: amberC; critColor: redC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: "Yağ Sıcaklığı"
                                unit: "°C"
                                value: p_YagSicakligi
                                maxY: selectedEngine === "TF10000" ? 160 : 130
                                lineColor: traceC; labelColor: cyanC; textNormal: whiteC; textDim: dimC
                            }
                            TrendGraph {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                label: "Titreşim"
                                unit: "IPS"
                                value: p_Titresim
                                maxY: selectedEngine === "TF10000" ? 16 : 4
                                warnAt: thTitresimWarn; critAt: thTitresimCrit
                                lineColor: traceC; labelColor: cyanC; warnColor: amberC; critColor: redC; textNormal: whiteC; textDim: dimC
                            }
                        }
                    }
                }
            }
        }
    }
}
