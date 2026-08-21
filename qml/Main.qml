import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    width: 1100
    height: 750
    visible: true
    title: "TEI Motor Telemetri İstasyonu"
    // Varsayilan pencere rengi beyaz - herhangi bir kare gecikmesinde/tam
    // kaplamayan bir alanda beyaz yerine koyu gorunsun diye acikca ayarlandi.
    color: "#121212"

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
            color: "#121212"
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 40

                Text {
                    text: "TEI TELEMETRİ SİSTEMİ BAŞLATILIYOR\nLütfen Test Edilecek Motoru Seçin"
                    color: "white"
                    font.pixelSize: 24; font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }

                RowLayout {
                    spacing: 40
                    Layout.alignment: Qt.AlignHCenter

                    Rectangle {
                        width: 300; height: 200
                        color: "#1e1e1e"; border.color: "#00ffcc"; border.width: 2; radius: 10
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 15
                            Text { text: "TF10000"; color: "#00ffcc"; font.pixelSize: 32; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                            Text { text: "Turbofan Motoru"; color: "gray"; font.pixelSize: 18; Layout.alignment: Qt.AlignHCenter }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                selectedEngine = "TF10000"
                                engineModel.selectEngine("TF10000")
                            }
                        }
                    }

                    Rectangle {
                        width: 300; height: 200
                        color: "#1e1e1e"; border.color: "#ff6600"; border.width: 2; radius: 10
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 15
                            Text { text: "PD170"; color: "#ff6600"; font.pixelSize: 32; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                            Text { text: "Turbodizel Havacılık Motoru"; color: "gray"; font.pixelSize: 18; Layout.alignment: Qt.AlignHCenter }
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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    TabBar {
                        id: tabBar
                        Layout.fillWidth: true
                        TabButton { text: "Ana Panel (" + selectedEngine + ")" }
                        TabButton { text: "Grafikler" }
                    }

                    // Motorun genel durumu - EngineModel::alarmLevel'dan tek kaynaktan okunur.
                    Rectangle {
                        Layout.preferredWidth: 110
                        Layout.preferredHeight: tabBar.height
                        color: engineModel.alarmLevelText === "KRİTİK" ? "#cc0000"
                             : engineModel.alarmLevelText === "UYARI"  ? "#ff9900"
                             : "#1a4c1a"
                        Text {
                            anchors.centerIn: parent
                            text: engineModel.alarmLevelText
                            color: "white"
                            font.bold: true
                            font.pixelSize: 14
                        }
                    }

                    Button {
                        text: "🔄 Motor Değiştir"
                        Layout.preferredHeight: tabBar.height
                        Layout.preferredWidth: 150

                        contentItem: Text {
                            text: parent.text; color: "white"; font.bold: true
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle { color: "#b30000" }

                        onClicked: {
                            selectedEngine = ""
                            isEngineOn = false
                            powerSlider.value = 0
                            tabBar.currentIndex = 0
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: tabBar.currentIndex

                    // SEKME 1: GÖSTERGELER
                    Rectangle {
                        color: "#1e1e1e"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 20

                            // 1. SOL BLOK: MOTOR START/STOP + FILO SECIMI
                            Rectangle {
                                Layout.preferredWidth: 230
                                Layout.fillHeight: true
                                color: "#2a2a2a"; border.color: "#444"; radius: 8
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 20; spacing: 14
                                    Text { text: selectedEngine + " KONTROL"; color: "white"; font.bold: true; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }

                                    // Secili filo motorunun uzun vadeli bakim durumu (asinmaya
                                    // dayali) - anlik alarm rozetinden (NORMAL/UYARI/KRITIK) FARKLI.
                                    Rectangle {
                                        Layout.fillWidth: true; Layout.preferredHeight: 26; radius: 4
                                        color: engineModel.maintenanceStatusText === "BAKIM GEREKLİ" ? "#cc0000"
                                             : engineModel.maintenanceStatusText === "İZLENMELİ"      ? "#ff9900"
                                             : "#1a4c1a"
                                        Text {
                                            anchors.centerIn: parent
                                            text: engineModel.maintenanceStatusText
                                            color: "white"; font.bold: true; font.pixelSize: 11
                                        }
                                    }

                                    Button {
                                        text: "MOTORU BAŞLAT"
                                        Layout.fillWidth: true; Layout.preferredHeight: 60; enabled: !isEngineOn
                                        onClicked: {
                                            isEngineOn = true
                                            engineModel.setPower(powerSlider.value)
                                            engineModel.startEngine()
                                        }
                                        contentItem: Text { text: parent.text; color: "white"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: parent.enabled ? "#33cc33" : "#1a4c1a"; radius: 5; border.color: "#111"; border.width: 2 }
                                    }
                                    Button {
                                        text: "MOTORU DURDUR"
                                        Layout.fillWidth: true; Layout.preferredHeight: 60; enabled: isEngineOn
                                        onClicked: {
                                            isEngineOn = false
                                            powerSlider.value = 0
                                            engineModel.stopEngine()
                                        }
                                        contentItem: Text { text: parent.text; color: "white"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { color: parent.enabled ? "#cc0000" : "#4c1a1a"; radius: 5; border.color: "#111"; border.width: 2 }
                                    }

                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#444" }

                                    Text { text: "FİLO (TEST İÇİN MOTOR SEÇ)"; color: "gray"; font.bold: true; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }

                                    // Yaslarina gore farkli asinmis 10 TF10000 - biri secilince
                                    // engineModel.selectFleetEngine(id) o motorun yipranmis
                                    // parametreleriyle simulasyonu sifirdan baslatir.
                                    ListView {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        clip: true
                                        spacing: 6
                                        model: engineModel.fleetEngines()

                                        delegate: Rectangle {
                                            width: ListView.view.width
                                            height: 44
                                            radius: 5
                                            color: modelData.id === selectedFleetId ? "#3a3a3a" : "#222"
                                            border.color: modelData.id === selectedFleetId ? "#00ffcc" : "#444"
                                            border.width: modelData.id === selectedFleetId ? 2 : 1

                                            ColumnLayout {
                                                anchors.fill: parent; anchors.margins: 6; spacing: 2
                                                Text { text: modelData.label; color: "white"; font.pixelSize: 11; font.bold: true }
                                                Text {
                                                    text: modelData.maintenanceStatusText
                                                    font.pixelSize: 10; font.bold: true
                                                    color: modelData.maintenanceStatusText === "BAKIM GEREKLİ" ? "#ff5555"
                                                         : modelData.maintenanceStatusText === "İZLENMELİ"      ? "#ffaa33"
                                                         : "#66dd66"
                                                }
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
                                }
                            }

                            // 2. ORTA BLOK: 8 PARAMETRE GÖSTERİMİ
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                spacing: 30

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
                                            property real warnStart: selectedEngine === "TF10000" ? 90 : 2500
                                            property real critStart: selectedEngine === "TF10000" ? 97 : 2750

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
                                                ctx.beginPath(); ctx.strokeStyle = "#33cc33"; ctx.arc(cx, cy, r, getAngle(min), getAngle(warnStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = "#ff9900"; ctx.arc(cx, cy, r, getAngle(warnStart), getAngle(critStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = "#cc0000"; ctx.arc(cx, cy, r, getAngle(critStart), getAngle(max)); ctx.stroke()

                                                // Ibrenin sablonu "yukari" (native 270 derece) cizildigi icin
                                                // rotate'e verilen aci bu dogal yonelimi telafi etmeli, yoksa
                                                // ibre gercek konumundan 270 derece (3/4 tur) kaymis durur.
                                                ctx.save(); ctx.translate(cx, cy); ctx.rotate(getAngle(value) - 1.5 * Math.PI)
                                                ctx.beginPath(); ctx.moveTo(-4, 0); ctx.lineTo(4, 0); ctx.lineTo(0, -(r - 2)); ctx.closePath()
                                                ctx.fillStyle = "white"; ctx.fill()
                                                ctx.beginPath(); ctx.arc(0, 0, 8, 0, 2*Math.PI); ctx.fillStyle = "#444"; ctx.fill()
                                                ctx.lineWidth = 2; ctx.strokeStyle = "white"; ctx.stroke(); ctx.restore()
                                            }
                                        }
                                        Text { text: selectedEngine === "TF10000" ? "N1 Fan" : "Motor Devri"; color: "gray"; font.pixelSize: 16; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_Devir1.toFixed(1) + (selectedEngine === "TF10000" ? " %" : " RPM"); color: "white"; font.pixelSize: 20; font.bold: true; Layout.alignment: Qt.AlignHCenter }
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
                                            property real warnStart: selectedEngine === "TF10000" ? 90 : 95
                                            property real critStart: selectedEngine === "TF10000" ? 97 : 102

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
                                                ctx.beginPath(); ctx.strokeStyle = "#33cc33"; ctx.arc(cx, cy, r, getAngle(min), getAngle(warnStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = "#ff9900"; ctx.arc(cx, cy, r, getAngle(warnStart), getAngle(critStart)); ctx.stroke()
                                                ctx.beginPath(); ctx.strokeStyle = "#cc0000"; ctx.arc(cx, cy, r, getAngle(critStart), getAngle(max)); ctx.stroke()

                                                // Ibrenin sablonu "yukari" (native 270 derece) cizildigi icin
                                                // rotate'e verilen aci bu dogal yonelimi telafi etmeli, yoksa
                                                // ibre gercek konumundan 270 derece (3/4 tur) kaymis durur.
                                                ctx.save(); ctx.translate(cx, cy); ctx.rotate(getAngle(value) - 1.5 * Math.PI)
                                                ctx.beginPath(); ctx.moveTo(-4, 0); ctx.lineTo(4, 0); ctx.lineTo(0, -(r - 2)); ctx.closePath()
                                                ctx.fillStyle = "white"; ctx.fill()
                                                ctx.beginPath(); ctx.arc(0, 0, 8, 0, 2*Math.PI); ctx.fillStyle = "#444"; ctx.fill()
                                                ctx.lineWidth = 2; ctx.strokeStyle = "white"; ctx.stroke(); ctx.restore()
                                            }
                                        }
                                        Text { text: selectedEngine === "TF10000" ? "N2 Core" : "Soğutma Suyu"; color: "gray"; font.pixelSize: 16; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_Devir2.toFixed(1) + (selectedEngine === "TF10000" ? " %" : " °C"); color: "white"; font.pixelSize: 20; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                    }
                                }

                                GridLayout {
                                    columns: 3
                                    rowSpacing: 40; columnSpacing: 40
                                    Layout.alignment: Qt.AlignHCenter

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        Text { text: selectedEngine === "TF10000" ? "Kompresör Basıncı" : "Manifold MAP"; color: "gray"; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_Basinc.toFixed(2) + " Bar"; color: "#00ffcc"; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                    }

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        Text { text: "EGT (Egzoz Sck.)"; color: "gray"; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_EGT.toFixed(0) + " °C"; color: "#ff9900"; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                    }

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        Text { text: "Yakıt Akışı"; color: "gray"; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_Yakit.toFixed(1) + " kg/h"; color: "white"; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                    }

                                    // param_YagBasinci (Taşma Hatası Çözüldü)
                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        Text { text: "Yağ Basıncı"; color: "gray"; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                        RowLayout {
                                            Rectangle {
                                                width: 20; height: 60; color: "#222"; border.color: "#555"; radius: 2

                                                Rectangle {
                                                    width: parent.width; anchors.bottom: parent.bottom

                                                    // Seçili motora göre maksimum basınç ayarı
                                                    property real maxPress: selectedEngine === "TF10000" ? 100.0 : 8.0

                                                    // Sütunun kaptan taşmasını engelleyen Math.min kilidi
                                                    height: Math.min((anim_YagBasinci / maxPress) * parent.height, parent.height)

                                                    // Seçilen motora göre kırmızı alarm bölgeleri
                                                    property bool isAlarm: selectedEngine === "TF10000" ? (anim_YagBasinci < 30.0 || anim_YagBasinci > 80.0) : (anim_YagBasinci < 1.5 || anim_YagBasinci > 6.0)

                                                    color: isAlarm ? "red" : "#00ffcc"
                                                    radius: 2
                                                }
                                            }
                                            Text {
                                                text: anim_YagBasinci.toFixed(1) + (selectedEngine === "TF10000" ? " psi" : " bar")
                                                color: "white"; font.pixelSize: 24; font.bold: true
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        Text { text: "Yağ Sıcaklığı"; color: "gray"; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_YagSicakligi.toFixed(1) + " °C"; color: "white"; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                    }

                                    ColumnLayout {
                                        Layout.alignment: Qt.AlignHCenter
                                        Text { text: "Titreşim"; color: "gray"; font.pixelSize: 14; Layout.alignment: Qt.AlignHCenter }
                                        Text { text: anim_Titresim.toFixed(2) + " IPS"; color: anim_Titresim > 4.0 ? "red" : "white"; font.pixelSize: 24; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                    }
                                }
                            }

                            // 3. SAĞ BLOK: GÜÇ / THROTTLE SLIDER
                            Rectangle {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true
                                color: "#2a2a2a"; border.color: "#444"; radius: 8

                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 20; spacing: 15
                                    Text { text: "THRUST"; color: "gray"; font.bold: true; font.pixelSize: 16; Layout.alignment: Qt.AlignHCenter }
                                    Text { text: "%" + powerSlider.value.toFixed(0); color: "white"; font.bold: true; font.pixelSize: 22; Layout.alignment: Qt.AlignHCenter }

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
                                            color: "#111"; border.color: "#555"; border.width: 3; radius: 4

                                            Column {
                                                anchors.fill: parent; anchors.margins: 4; spacing: 3
                                                Repeater {
                                                    model: 10
                                                    Rectangle {
                                                        id: segmentBg
                                                        width: parent.width; height: (parent.height - 27) / 10
                                                        color: "#222"

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
                                                            color: index < 3 ? "#ff6600" : "#33cc33"
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        handle: Rectangle {
                                            x: powerSlider.leftPadding + (powerSlider.availableWidth - width) / 2
                                            y: powerSlider.topPadding + powerSlider.visualPosition * (powerSlider.availableHeight - height)
                                            width: 70; height: 15; color: powerSlider.enabled ? "#ccc" : "#555"; border.color: "#333"; border.width: 2; radius: 3
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // SEKME 2: GRAFİKLER
                    Rectangle {
                        color: "#1e1e1e"
                        Text { anchors.centerIn: parent; text: "Grafikler Burada Olacak"; color: "gray"; font.pixelSize: 20 }
                    }
                }
            }
        }
    }
}