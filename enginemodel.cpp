#include "enginemodel.h"                                //Bridge / ViewMode   src
#include "embeddedtempsensor.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QVariantMap>
#include <QVector>
#include <algorithm>

namespace { // sadece bu .cpp dosyasından erişilebilir

struct FleetEntry { int id; float ageYears; };
constexpr FleetEntry kFleet[] = {
    {1, 0.0f}, {2, 1.0f}, {3, 2.0f}, {4, 3.0f}, {5, 5.0f},
    {6, 7.0f}, {7, 10.0f}, {8, 13.0f}, {9, 16.0f}, {10, 20.0f}
};

// Bakım durumunu QML arayüzünde gösterilecek metinlere çevirir.
QString maintenanceStatusToText(MaintenanceStatus status) {
    switch (status) {
    case MaintenanceStatus::Watch:              return QStringLiteral("İZLENMELİ");
    case MaintenanceStatus::MaintenanceRequired: return QStringLiteral("BAKIM GEREKLİ");
    default:                                     return QStringLiteral("SAĞLIKLI");
    }
}

// %10'dan fazla sapan değerleri filtreler ve QML'de solda göstericez
QVariantList buildWearNotes(const WearDeviations &d) {
    struct Item { double pct; QString label; };
    QVector<Item> items = {
                            {d.titresimPct,     QStringLiteral("Titreşim")},
                            {d.yagBasinciPct,   QStringLiteral("Yağ Basıncı")},
                            {d.egtPct,          QStringLiteral("EGT")},
                            {d.yagSicakligiPct, QStringLiteral("Yağ Sıcaklığı")},
                            {d.yakitPct,        QStringLiteral("Yakıt Akışı")},
                            };
    // Sapma oranına göre büyükten küçüğe sıralar.
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        return qAbs(a.pct) > qAbs(b.pct);
    });

    const double threshold = 10.0;
    QVariantList notes;
    for (const auto &it : items) {
        if (qAbs(it.pct) < threshold) continue;
        const QString sign = it.pct >= 0 ? QStringLiteral("+") : QString();
        notes << QStringLiteral("%1: referansa göre %2%3%")
                     .arg(it.label, sign, QString::number(it.pct, 'f', 0));
    }
    return notes;
}

//  Parametrenin karakterine göre rastgele dalgalanma (gürültü) üretir.
double jitterFactor(double spreadPercent) {
    const double spread = spreadPercent / 100.0;
    return 1.0 + (QRandomGenerator::global()->generateDouble() - 0.5) * 2.0 * spread;
}

// Fiziksel atalet hesabı. Mevcut değeri hedef yavaşca yaklaştırır.
double approachFactor(double current, double target, double riseSeconds, double fallSeconds, double tickSeconds) {
    const double timeConstant = (target > current) ? riseSeconds : fallSeconds;
    const double step = tickSeconds / qMax(timeConstant, 0.001);
    return current + qBound(-step, target - current, step);
}
}

//Constructor
EngineModel::EngineModel(QObject *parent) : QObject(parent) {
    m_embeddedSensor = new EmbeddedTempSensor(this);
    connect(m_embeddedSensor, &EmbeddedTempSensor::temperatureUpdated, this, &EngineModel::onEmbeddedTempUpdated);
    connect(m_embeddedSensor, &EmbeddedTempSensor::connectionStateChanged, this, &EngineModel::onEmbeddedConnectionChanged);

    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &EngineModel::simulationTick); //Qt'nin Signal kur
    m_simTimer->start(200);  //0.2 saniye loop

}

// Gomulu (STM32 dahili) sensorden gelen ham sicakligi Q_PROPERTY'ye yansitir - bu
// deger ortam degil, engine_core.h::EGT_SENSOR_GAIN ile motora ozgu oranlanarak
// dogrudan EGT'ye yansir.
void EngineModel::onEmbeddedTempUpdated(double celsius) {
    m_sensorSicakligi = celsius;
    emit sensorSicakligiChanged();
}
void EngineModel::onEmbeddedConnectionChanged(bool connected) {
    m_sensorBagli = connected;
    emit sensorBagliChanged();
}
//destructor
EngineModel::~EngineModel() {
    delete currentEngine; // Bellek sızıntısını önler.
}

// Polimorfizm devrede: Seçilen isme göre doğru motor sınıfı yaratılır.      3
void EngineModel::selectEngine(const QString &engineName) {
    // Tanınmayan isim gelirse önceki motoru silmeden sessizce reddet - aksi halde
    // currentEngine nullptr kalır, simulationTick() no-op'a düşer ve UI son gördüğü
    // değerlerde donup kalır (kullanıcıya hata gitmeden "hayalet" bir durum oluşur).
    if (engineName != "TF10000" && engineName != "PD170") return;

    delete currentEngine;
    currentEngine = nullptr;
    m_engineFamily = engineName; // Filo seçiminde hangi sınıfı yaratacağımızı burada hatırlarız.

    if (engineName == "TF10000") currentEngine = new TF10000();
    else currentEngine = new PD170();

    resetSimulationStateFor(currentEngine);
}

// QML arayüzündeki filo listesini dolduran fonksiyon.
QVariantList EngineModel::fleetEngines() const {
    QVariantList list;
    for (const auto &entry : kFleet) {
        QVariantMap m;
        m["id"] = entry.id;
        m["ageYears"] = entry.ageYears;
        m["label"] = entry.ageYears <= 0.0f
                         ? QStringLiteral("Motor #%1 (Sıfır km)").arg(entry.id)
                         : QStringLiteral("Motor #%1 (%2 yıl)").arg(entry.id).arg(entry.ageYears, 0, 'f', 0);
        list.append(m);
    }
    return list;
}
//filo listesinden bir motor seçildiğinde çalışır           4
void EngineModel::selectFleetEngine(int fleetId) {
    float ageYears = 0.0f;
    bool found = false;
    for (const auto &entry : kFleet) {
        if (entry.id == fleetId) { ageYears = entry.ageYears; found = true; break; }
    }
    if (!found) return;

    delete currentEngine;
    // m_engineFamily hangi motor tipinin filosundan seçim yaptığımızı soyler -
    // PD170 icin secilmemis/bilinmiyorsa guvenli varsayilan olarak TF10000.
    if (m_engineFamily == "PD170") currentEngine = new PD170(ageYears);
    else currentEngine = new TF10000(ageYears); // Yaş verisiyle yıpranmış motor simülasyonu başlar.
    resetSimulationStateFor(currentEngine);  //en son reset
}


// Motor değiştiğinde verileri sıfırlar
void EngineModel::resetSimulationStateFor(Engine *newEngine) {
    m_running = false;
    m_targetPower = 0.0;
    m_actualPower = 0.0;
    m_factorDevir1 = 0.0;
    m_factorDevir2 = 0.0;
    m_factorBasinc = 0.0;
    m_factorEgt = 0.0;
    m_factorYakit = 0.0;
    m_factorYagBasinci = 0.0;
    m_factorYagSicakligi = 0.0;
    m_factorTitresim = 0.0;
    m_tested = false;

    if (newEngine) {
        newEngine->Engine_Start(0.0, m_sensorSicakligi); // sensor bagliysa gercek, degilse varsayilan 25 derece
        refreshFromEngine(/*withJitter=*/false); // İlk frame'i temiz (gürültüsüz) çizer.

        m_maintenanceStatusText = maintenanceStatusToText(newEngine->GetMaintenanceStatus());
        m_wearNotes = buildWearNotes(newEngine->GetWearDeviations(0.0f));
        emit maintenanceStatusChanged();
    }
}

// Hedef gücü ayarlar (Örneğin QML'deki bir Slider'dan gelir).
void EngineModel::setPower(double powerPercent) { m_targetPower = qBound(0.0, powerPercent, 100.0); } // UI'dan gelen aşırı/negatif değer m_actualPower'ın sınırsız sürünmesini önler.
void EngineModel::startEngine() { m_running = true; }      //motor başlatır            5
void EngineModel::stopEngine() {                    //9
    m_running = false;
    m_targetPower = 0.0;
}

// SİSTEMİN KALBİ: QTimer tarafından saniyede 5 kez çağrılır.                    6
void EngineModel::simulationTick() {
    if (!currentEngine) return;

    const double spoolTarget = m_running ? 1.0 : 0.0;
    const double tick = 0.2; // 200ms = 0.2 saniye
    const SpoolProfile p = currentEngine->GetSpoolProfile();

    // Motor parametrelerinin fiziksel ataletlerini hesaplar.
    m_factorDevir1       = approachFactor(m_factorDevir1,       spoolTarget, p.devir1Rise,       p.devir1Fall,       tick);
    m_factorDevir2       = approachFactor(m_factorDevir2,       spoolTarget, p.devir2Rise,       p.devir2Fall,       tick);
    m_factorBasinc       = approachFactor(m_factorBasinc,       spoolTarget, p.basincRise,       p.basincFall,       tick);
    m_factorEgt          = approachFactor(m_factorEgt,          spoolTarget, p.egtRise,          p.egtFall,          tick);
    m_factorYakit        = approachFactor(m_factorYakit,        spoolTarget, p.yakitRise,        p.yakitFall,        tick);
    m_factorYagBasinci   = approachFactor(m_factorYagBasinci,   spoolTarget, p.yagBasinciRise,   p.yagBasinciFall,   tick);
    m_factorYagSicakligi = approachFactor(m_factorYagSicakligi, spoolTarget, p.yagSicakligiRise, p.yagSicakligiFall, tick);
    m_factorTitresim     = approachFactor(m_factorTitresim,     spoolTarget, p.titresimRise,     p.titresimFall,     tick);

    // Gerçekleşen güç, gaz kolu hedefine doğru yavaşca ilerler.
    const double powerStep = 4.0;
    m_actualPower += qBound(-powerStep, m_targetPower - m_actualPower, powerStep);


    currentEngine->Engine_Start(m_actualPower / 100.0, m_sensorSicakligi);// Çekirdek motor modeli güncellenir - Gomulu.cpp.txt'den gelen ham ADC sicakligi EGT'ye motora ozgu oranla yansir (yoksa varsayilan 25).
    refreshFromEngine(/*withJitter=*/m_factorDevir1 > 0.01 || m_factorEgt > 0.01);    // Motor duruyorken sensör titremesi engeller

    //  Yağ basıncı oturana kadar test tamamlanmış sayılmaz.  sonra test biter               7
    if (m_running && m_factorYagBasinci >= 0.97 && !m_tested) {  //gürültü payıda var %97
        m_tested = true;
        emit maintenanceStatusChanged();
    }

    // Gaz kolu (power) değiştikçe yıpranma sapmasını hesaplar                           8
    if (m_tested) {
        const QVariantList freshNotes = buildWearNotes(currentEngine->GetWearDeviations(static_cast<float>(m_actualPower / 100.0)));
        if (freshNotes != m_wearNotes) {
            m_wearNotes = freshNotes;
            emit maintenanceStatusChanged();
        }
    }
}

// Fiziksel değerleri UI  değerlerine çevirir.
void EngineModel::refreshFromEngine(bool withJitter) {
    double devir1       = currentEngine->param_Devir1;
    double devir2       = currentEngine->param_Devir2;
    double basinc       = currentEngine->param_Basinc;
    double egt          = currentEngine->param_EGT;
    double yakit        = currentEngine->param_Yakit;
    double yagBasinci   = currentEngine->param_YagBasinci;
    double yagSicakligi = currentEngine->param_YagSicakligi;
    double titresim     = currentEngine->param_Titresim;

    if (withJitter) { // Her sensörün kendi karakteristiğine göre gürültü eklenir.
        devir1       *= jitterFactor(0.8);
        devir2       *= jitterFactor(0.8);
        basinc       *= jitterFactor(1.5);
        egt          *= jitterFactor(1.0);
        yakit        *= jitterFactor(1.5);
        yagBasinci   *= jitterFactor(1.5);
        yagSicakligi *= jitterFactor(0.3);
        titresim     *= jitterFactor(6.0);
    }

    // Atalet çarpanları ile nihai gösterim değerleri çarpılır.
    devir1       *= m_factorDevir1;
    devir2       *= m_factorDevir2;
    basinc       *= m_factorBasinc;
    egt          *= m_factorEgt;
    yakit        *= m_factorYakit;
    yagBasinci   *= m_factorYagBasinci;
    yagSicakligi *= m_factorYagSicakligi;
    titresim     *= m_factorTitresim;

    // Gürültü yüzünden fiziksel tavanın aşılması  engellenir.
    const ParamCeilings ceil = currentEngine->GetParamCeilings();
    devir1       = qMin(devir1,       ceil.devir1);
    devir2       = qMin(devir2,       ceil.devir2);
    basinc       = qMin(basinc,       ceil.basinc);
    egt          = qMin(egt,          ceil.egt);
    yakit        = qMin(yakit,        ceil.yakit);
    yagBasinci   = qMin(yagBasinci,   ceil.yagBasinci);
    yagSicakligi = qMin(yagSicakligi, ceil.yagSicakligi);
    titresim     = qMin(titresim,     ceil.titresim);

    // Q_PROPERTY setter fonksiyonları çağrılır, böylece QML'e "değer değişti" sinyalleri gider.
    setDevir1(devir1);
    setDevir2(devir2);
    setBasinc(basinc);
    setEgt(egt);
    setYakit(yakit);
    setYagBasinci(yagBasinci);
    setYagSicakligi(yagSicakligi);
    setTitresim(titresim);

    // Çekirdek modele gürültülü ve ataletli değerleri geri besliyoruz ki alarm değerlendirmesi doğru çalışsın.
    currentEngine->param_Devir1       = static_cast<float>(devir1);
    currentEngine->param_Devir2       = static_cast<float>(devir2);
    currentEngine->param_Basinc       = static_cast<float>(basinc);
    currentEngine->param_EGT          = static_cast<float>(egt);
    currentEngine->param_Yakit        = static_cast<float>(yakit);
    currentEngine->param_YagBasinci   = static_cast<float>(yagBasinci);
    currentEngine->param_YagSicakligi = static_cast<float>(yagSicakligi);
    currentEngine->param_Titresim     = static_cast<float>(titresim);

    // Motor çalışırken yağ basıncı henüz tam oturmadıysa, geçici "düşük basınç" alarmı ver
    if (m_factorYagBasinci >= 0.97) {
        setAlarmLevel(static_cast<AlarmLevel>(currentEngine->EvaluateAlarm()));
    } else {
        setAlarmLevel(AlarmLevel::Normal);
    }
}

QString EngineModel::alarmLevelText() const {
    switch (m_alarmLevel) {
    case AlarmLevel::Warning:  return QStringLiteral("UYARI");
    case AlarmLevel::Critical: return QStringLiteral("KRİTİK");
    default:                   return QStringLiteral("NORMAL");
    }
}

void EngineModel::setAlarmLevel(AlarmLevel v) {
    if (m_alarmLevel != v) {
        m_alarmLevel = v;
        emit alarmLevelChanged();
    }
}

// UI  sadece değer gerçekten değiştiğinde sinyal (emit) fırlatır
//0 a yakınlık için kullandık
// +1.0 eklenmesinin sebebi: qFuzzyCompare fonksiyonunun 0'a çok yakın kayan noktalı sayılarda hata vermesini önlemektir.
void EngineModel::setDevir1(double v)       { if (!qFuzzyCompare(m_devir1 + 1.0, v + 1.0))       { m_devir1 = v;       emit devir1Changed(); } }
void EngineModel::setDevir2(double v)       { if (!qFuzzyCompare(m_devir2 + 1.0, v + 1.0))       { m_devir2 = v;       emit devir2Changed(); } }
void EngineModel::setBasinc(double v)       { if (!qFuzzyCompare(m_basinc + 1.0, v + 1.0))       { m_basinc = v;       emit basincChanged(); } }
void EngineModel::setEgt(double v)          { if (!qFuzzyCompare(m_egt + 1.0, v + 1.0))          { m_egt = v;          emit egtChanged(); } }
void EngineModel::setYakit(double v)        { if (!qFuzzyCompare(m_yakit + 1.0, v + 1.0))        { m_yakit = v;        emit yakitChanged(); } }
void EngineModel::setYagBasinci(double v)   { if (!qFuzzyCompare(m_yagBasinci + 1.0, v + 1.0))   { m_yagBasinci = v;   emit yagBasinciChanged(); } }
void EngineModel::setYagSicakligi(double v) { if (!qFuzzyCompare(m_yagSicakligi + 1.0, v + 1.0)) { m_yagSicakligi = v; emit yagSicakligiChanged(); } }
void EngineModel::setTitresim(double v)     { if (!qFuzzyCompare(m_titresim + 1.0, v + 1.0))     { m_titresim = v;     emit titresimChanged(); } }