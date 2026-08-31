#ifndef EMBEDDEDTEMPSENSOR_H
#define EMBEDDEDTEMPSENSOR_H

#include <QObject>
#include <QByteArray>

class QSerialPort;
class QTimer;

// STM32 karti (bkz. Gomulu.cpp.txt) UART'tan "TEMP:24.53\n" formatinda sicaklik
// gonderiyor. Kaynak, harici bir LM35 DEGIL: kodda kullanilan V25=760mV /
// AVG_SLOPE=2.5mV/°C sabitleri, STM32F4 serisinin REFERANS KILAVUZUNDA (RM0090)
// tanimli DAHILI (chip ici) sicaklik sensorunun tipik karakterizasyon degerleriyle
// birebir eslesiyor - yani bu, MCU'nun kendi die sicakligi (oda havasi degil, ic
// ozisinma dahil). Bu sinif COM portunu bulup baglaniyor, gelen satirlari
// ayristirip EngineModel'e besliyor. EngineModel bu degeri EGT'ye motora ozgu
// oranlanarak yansiyan bir girdi olarak kullaniyor (bkz. engine_core.h
// EGT_SENSOR_GAIN) - bu sinif sadece ham veriyi tasir, yorumlamaz.
// Kart takili degilse/COM portu yoksa sessizce devre disi kalir; uygulama
// varsayilan (25 derece, gomulu tarafin SIMULASYON_MODU=1'iyle ayni deger) ile
// normal calismaya devam eder - donanim bu proje icin opsiyonel bir girdi.
class EmbeddedTempSensor : public QObject {
    Q_OBJECT
public:
    explicit EmbeddedTempSensor(QObject *parent = nullptr);

    double lastTemperature() const { return m_lastTemp; }
    bool isConnected() const { return m_connected; }

signals:
    void temperatureUpdated(double celsius);
    void connectionStateChanged(bool connected);

private slots:
    // Baglanti yoksa periyodik olarak tekrar dener (kart sonradan takilabilir).
    void tryAutoConnect();
    void onReadyRead();

private:
    void openPort(const QString &portName);
    void handleLine(const QByteArray &line);
    void setConnected(bool connected);

    QSerialPort *m_port = nullptr;
    QTimer *m_retryTimer = nullptr;
    QByteArray m_buffer;
    double m_lastTemp = 25.0; // kart baglanana kadar / baglanti kesilirse kullanilan guvenli varsayilan
    bool m_connected = false;
};

#endif // EMBEDDEDTEMPSENSOR_H
