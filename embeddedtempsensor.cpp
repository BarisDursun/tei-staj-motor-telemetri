#include "embeddedtempsensor.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>

namespace {
// Gomulu ekibin main.cpp'sinde (Gomulu.cpp.txt) UART baud rate CubeMX tarafinda
// ayarlaniyor ve bize gelen dosyada gorunmuyor - 115200, STM32 projelerinde en
// yaygin varsayilan. Kart baglaniyor ama hic "TEMP:" satiri gelmiyorsa once
// burayi ve teammate'in CubeMX USART2 ayarini karsilastirin.
constexpr qint32 BAUD_RATE = 115200;
constexpr int RETRY_INTERVAL_MS = 3000;
}

EmbeddedTempSensor::EmbeddedTempSensor(QObject *parent) : QObject(parent) {
    m_retryTimer = new QTimer(this);
    connect(m_retryTimer, &QTimer::timeout, this, &EmbeddedTempSensor::tryAutoConnect);
    m_retryTimer->start(RETRY_INTERVAL_MS);
    tryAutoConnect(); // acilista hemen bir kez dene, ilk 3sn'yi bosa harcamayalim
}

void EmbeddedTempSensor::tryAutoConnect() {
    if (m_connected) return; // zaten bagli - tekrar taramaya gerek yok

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        // STM32 Nucleo/Discovery kartlari ST-Link uzerinden "STMicroelectronics"
        // markali bir sanal COM portu (VCP) acar - varsa once o denenir.
        if (info.manufacturer().contains("STMicroelectronics", Qt::CaseInsensitive)) {
            openPort(info.portName());
            return;
        }
    }
    // Tam eslesme yok: masaustunde baska seri port olmasi az rastlanir bir durum
    // oldugu icin, listede TEK bir port varsa (USB-UART koprusu gibi) yine deneriz.
    // Birden fazla belirsiz port varsa yanlis cihaza baglanmamak icin denemeyiz.
    if (ports.size() == 1) {
        openPort(ports.first().portName());
    }
}

void EmbeddedTempSensor::openPort(const QString &portName) {
    delete m_port;
    m_port = new QSerialPort(this);
    m_port->setPortName(portName);
    m_port->setBaudRate(BAUD_RATE);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadOnly)) {
        delete m_port;
        m_port = nullptr;
        return;
    }

    connect(m_port, &QSerialPort::readyRead, this, &EmbeddedTempSensor::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError) return;
        m_port->close(); // kablo cekildi/kart resetlendi gibi - kapat, retryTimer tekrar dener
        setConnected(false);
    });
    // Not: "bagli" durumu port acilinca degil, ilk gecerli "TEMP:" satiri basariyla
    // ayristirilinca true olur (bkz. handleLine) - yanlis porta baglanip bos/bozuk
    // veri aliyor olmayi "sensor calisiyor" gibi gostermemek icin.
}

void EmbeddedTempSensor::onReadyRead() {
    m_buffer += m_port->readAll();
    int newlineIndex;
    while ((newlineIndex = m_buffer.indexOf('\n')) != -1) {
        const QByteArray line = m_buffer.left(newlineIndex).trimmed();
        m_buffer.remove(0, newlineIndex + 1);
        handleLine(line);
    }
    // Hicbir zaman '\n' gelmiyorsa (yanlis baud rate ihtimali) tampon sinirsiz
    // buyumesin diye savunma amacli sinirlandirildi.
    if (m_buffer.size() > 256) m_buffer.clear();
}

void EmbeddedTempSensor::handleLine(const QByteArray &line) {
    if (!line.startsWith("TEMP:")) return; // Gomulu.cpp.txt formati: "TEMP:24.53"

    bool ok = false;
    const double value = line.mid(5).toDouble(&ok);
    if (!ok) return;

    m_lastTemp = value;
    emit temperatureUpdated(value);
    setConnected(true); // ilk gecerli satirda baglanti fiilen dogrulanmis olur
}

void EmbeddedTempSensor::setConnected(bool connected) {
    if (m_connected == connected) return;
    m_connected = connected;
    emit connectionStateChanged(connected);
}
