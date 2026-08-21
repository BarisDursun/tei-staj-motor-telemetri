#include "enginemodel.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QVariantMap>

namespace {

// TF10000 filosu: 0 (sifir km) ile 20 yil arasi cesitli yaslarda 10 sabit
// motor - test senaryolarinin hem "saglikli" hem "bakim gerekli" ornekler
// icermesi icin secildi (bkz. Engine::WearFactor yorumu).
struct FleetEntry { int id; float ageYears; };
constexpr FleetEntry kFleet[] = {
    {1, 0.0f}, {2, 1.0f}, {3, 2.0f}, {4, 3.0f}, {5, 5.0f},
    {6, 7.0f}, {7, 10.0f}, {8, 13.0f}, {9, 16.0f}, {10, 20.0f}
};

QString maintenanceStatusToText(MaintenanceStatus status) {
    switch (status) {
    case MaintenanceStatus::Watch:              return QStringLiteral("İZLENMELİ");
    case MaintenanceStatus::MaintenanceRequired: return QStringLiteral("BAKIM GEREKLİ");
    default:                                     return QStringLiteral("SAĞLIKLI");
    }
}
// Parametreye gore farkli buyuklukte rastgele carpan uretir (yuzde cinsinden
// yayilim). Termal kutlesi yuksek olan parametreler (yag sicakligi) gercekte
// yavas degisir, titresim ise dogasi geregi en gurultulu olanidir - o yuzden
// tum parametrelere ayni sabit gurultu uygulanmiyor.
double jitterFactor(double spreadPercent) {
    const double spread = spreadPercent / 100.0;
    return 1.0 + (QRandomGenerator::global()->generateDouble() - 0.5) * 2.0 * spread;
}

// Bir carpani (0..1) hedefe dogru, yon bazinda FARKLI zaman sabitiyle
// (rise/fall, saniye) ilerletir - gercek motorlarda yukselis ve dusus
// ayni hizda olmadigi icin (bkz. engine_core.h SpoolProfile yorumu).
double approachFactor(double current, double target, double riseSeconds, double fallSeconds, double tickSeconds) {
    const double timeConstant = (target > current) ? riseSeconds : fallSeconds;
    const double step = tickSeconds / qMax(timeConstant, 0.001);
    return current + qBound(-step, target - current, step);
}
}

EngineModel::EngineModel(QObject *parent) : QObject(parent) {
    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &EngineModel::simulationTick);
    m_simTimer->start(200);
}

EngineModel::~EngineModel() {
    delete currentEngine;
}

void EngineModel::selectEngine(const QString &engineName) {
    delete currentEngine;
    currentEngine = nullptr;

    if (engineName == "TF10000") currentEngine = new TF10000();
    else if (engineName == "PD170") currentEngine = new PD170();

    resetSimulationStateFor(currentEngine);
}

QVariantList EngineModel::fleetEngines() const {
    QVariantList list;
    for (const auto &entry : kFleet) {
        TF10000 probe(entry.ageYears);
        QVariantMap m;
        m["id"] = entry.id;
        m["ageYears"] = entry.ageYears;
        m["label"] = entry.ageYears <= 0.0f
            ? QStringLiteral("Motor #%1 (Sıfır km)").arg(entry.id)
            : QStringLiteral("Motor #%1 (%2 yıl)").arg(entry.id).arg(entry.ageYears, 0, 'f', 0);
        m["maintenanceStatusText"] = maintenanceStatusToText(probe.GetMaintenanceStatus());
        list.append(m);
    }
    return list;
}

void EngineModel::selectFleetEngine(int fleetId) {
    float ageYears = 0.0f;
    bool found = false;
    for (const auto &entry : kFleet) {
        if (entry.id == fleetId) { ageYears = entry.ageYears; found = true; break; }
    }
    if (!found) return;

    delete currentEngine;
    currentEngine = new TF10000(ageYears);
    resetSimulationStateFor(currentEngine);
}

void EngineModel::resetSimulationStateFor(Engine *newEngine) {
    // Yeni secilen motor tamamen kapali durumda baslar - onceki motordan
    // kalma degerlerin bir sonraki tike kadar ekranda kalmamasi icin
    // durumu sifirlayip hemen bir kez yeniliyoruz.
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

    if (newEngine) {
        newEngine->Engine_Start(0.0, 25.0);
        refreshFromEngine(/*withJitter=*/false);

        const QString newText = maintenanceStatusToText(newEngine->GetMaintenanceStatus());
        if (newText != m_maintenanceStatusText) {
            m_maintenanceStatusText = newText;
            emit maintenanceStatusChanged();
        }
    }
}

void EngineModel::setPower(double powerPercent) {
    m_targetPower = powerPercent;
}

void EngineModel::startEngine() {
    m_running = true;
}

void EngineModel::stopEngine() {
    m_running = false;
    m_targetPower = 0.0;
}

void EngineModel::simulationTick() {
    if (!currentEngine) return;

    const double spoolTarget = m_running ? 1.0 : 0.0;
    const double tick = 0.2; // m_simTimer araligi (saniye)
    const SpoolProfile p = currentEngine->GetSpoolProfile();

    m_factorDevir1       = approachFactor(m_factorDevir1,       spoolTarget, p.devir1Rise,       p.devir1Fall,       tick);
    m_factorDevir2       = approachFactor(m_factorDevir2,       spoolTarget, p.devir2Rise,       p.devir2Fall,       tick);
    m_factorBasinc       = approachFactor(m_factorBasinc,       spoolTarget, p.basincRise,       p.basincFall,       tick);
    m_factorEgt          = approachFactor(m_factorEgt,          spoolTarget, p.egtRise,          p.egtFall,          tick);
    m_factorYakit        = approachFactor(m_factorYakit,        spoolTarget, p.yakitRise,        p.yakitFall,        tick);
    m_factorYagBasinci   = approachFactor(m_factorYagBasinci,   spoolTarget, p.yagBasinciRise,   p.yagBasinciFall,   tick);
    m_factorYagSicakligi = approachFactor(m_factorYagSicakligi, spoolTarget, p.yagSicakligiRise, p.yagSicakligiFall, tick);
    m_factorTitresim     = approachFactor(m_factorTitresim,     spoolTarget, p.titresimRise,     p.titresimFall,     tick);

    // Gerceklesen guc, gaz kolu hedefine dogru asamali yaklasir (throttle tepkisi
    // mekanik spool'dan daha hizli - gercek bir turbinde de guc degisimi boyle olur).
    const double powerStep = 4.0;
    m_actualPower += qBound(-powerStep, m_targetPower - m_actualPower, powerStep);

    currentEngine->Engine_Start(m_actualPower / 100.0, 25.0);
    refreshFromEngine(/*withJitter=*/m_factorDevir1 > 0.01 || m_factorEgt > 0.01);
}

void EngineModel::refreshFromEngine(bool withJitter) {
    double devir1       = currentEngine->param_Devir1;
    double devir2       = currentEngine->param_Devir2;
    double basinc       = currentEngine->param_Basinc;
    double egt          = currentEngine->param_EGT;
    double yakit        = currentEngine->param_Yakit;
    double yagBasinci   = currentEngine->param_YagBasinci;
    double yagSicakligi = currentEngine->param_YagSicakligi;
    double titresim     = currentEngine->param_Titresim;

    if (withJitter) {
        devir1       *= jitterFactor(0.8);
        devir2       *= jitterFactor(0.8);
        basinc       *= jitterFactor(1.5);
        egt          *= jitterFactor(1.0);
        yakit        *= jitterFactor(1.5);
        yagBasinci   *= jitterFactor(1.5);
        yagSicakligi *= jitterFactor(0.3);
        titresim     *= jitterFactor(6.0);
    }

    // Motor kapaliyken/spool asamasindayken her deger KENDI carpanina gore
    // olceklenir - her parametrenin gercek hayattaki ataleti/isil kutlesi farkli
    // oldugu icin farkli hizda spool oluyor (bkz. engine_core.h SpoolProfile).
    devir1       *= m_factorDevir1;
    devir2       *= m_factorDevir2;
    basinc       *= m_factorBasinc;
    egt          *= m_factorEgt;
    yakit        *= m_factorYakit;
    yagBasinci   *= m_factorYagBasinci;
    yagSicakligi *= m_factorYagSicakligi;
    titresim     *= m_factorTitresim;

    // Jitter (gurultu) %100 guctekiyle carpilinca, tavan degerin hafifce
    // ustune cikabilir (orn. "%100.2" gibi fiziksel olarak anlamsiz bir
    // okuma) - motorun gercek tavanini asamayacak sekilde sinirliyoruz.
    const ParamCeilings ceil = currentEngine->GetParamCeilings();
    devir1       = qMin(devir1,       ceil.devir1);
    devir2       = qMin(devir2,       ceil.devir2);
    basinc       = qMin(basinc,       ceil.basinc);
    egt          = qMin(egt,          ceil.egt);
    yakit        = qMin(yakit,        ceil.yakit);
    yagBasinci   = qMin(yagBasinci,   ceil.yagBasinci);
    yagSicakligi = qMin(yagSicakligi, ceil.yagSicakligi);
    titresim     = qMin(titresim,     ceil.titresim);

    setDevir1(devir1);
    setDevir2(devir2);
    setBasinc(basinc);
    setEgt(egt);
    setYakit(yakit);
    setYagBasinci(yagBasinci);
    setYagSicakligi(yagSicakligi);
    setTitresim(titresim);

    // Alarm degerlendirmesi de gosterilen (spool+jitter uygulanmis) degerlerle
    // tutarli olsun diye Engine'in kendi alanlarini gosterilen degerlere esitleyip
    // oyle EvaluateAlarm() cagiriyoruz.
    currentEngine->param_Devir1       = static_cast<float>(devir1);
    currentEngine->param_Devir2       = static_cast<float>(devir2);
    currentEngine->param_Basinc       = static_cast<float>(basinc);
    currentEngine->param_EGT          = static_cast<float>(egt);
    currentEngine->param_Yakit        = static_cast<float>(yakit);
    currentEngine->param_YagBasinci   = static_cast<float>(yagBasinci);
    currentEngine->param_YagSicakligi = static_cast<float>(yagSicakligi);
    currentEngine->param_Titresim     = static_cast<float>(titresim);

    // Alarm degerlendirmesi SADECE motor "oturmus" sayilacak kadar spool
    // olduysa yapilir (yag pompasi spool'u >= %60). Aksi halde iki durumda
    // yanlis KRITIK cikardi: (1) motor tamamen kapaliyken yag basinci
    // gercekci olarak 0 - "dusuk yag basinci" bant kontrolu bunu ariza sanir,
    // (2) motor YENI baslatilirken (spool-up devam ederken) yag pompasi
    // henuz tam hiza ulasmadigi icin basinc gecici olarak dusuk - gercek bir
    // motorda da start sirasinda birkac saniye "dusuk yag basinci" alarmi
    // verilmez, pompanin oturmasi beklenir. m_running=false oldugunda zaten
    // spool geriye gidip bu esigin altina duser, o yuzden ayrica kontrol
    // etmeye gerek yok.
    if (m_factorYagBasinci >= 0.60) {
        // EngineAlarmLevel ve AlarmLevel ayni sirada tanimli (Normal/Warning/Critical),
        // Engine sinifi Qt'ye bagimli olmasin diye kendi enum'unu dondurur.
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

// +1.0 kaydirmasi qFuzzyCompare'in sifira yakin degerlerde saglikli
// calismamasini (relative fark tanimsizlasir) onlemek icin kullanilir.
void EngineModel::setDevir1(double v)       { if (!qFuzzyCompare(m_devir1 + 1.0, v + 1.0))       { m_devir1 = v;       emit devir1Changed(); } }
void EngineModel::setDevir2(double v)       { if (!qFuzzyCompare(m_devir2 + 1.0, v + 1.0))       { m_devir2 = v;       emit devir2Changed(); } }
void EngineModel::setBasinc(double v)       { if (!qFuzzyCompare(m_basinc + 1.0, v + 1.0))       { m_basinc = v;       emit basincChanged(); } }
void EngineModel::setEgt(double v)          { if (!qFuzzyCompare(m_egt + 1.0, v + 1.0))          { m_egt = v;          emit egtChanged(); } }
void EngineModel::setYakit(double v)        { if (!qFuzzyCompare(m_yakit + 1.0, v + 1.0))        { m_yakit = v;        emit yakitChanged(); } }
void EngineModel::setYagBasinci(double v)   { if (!qFuzzyCompare(m_yagBasinci + 1.0, v + 1.0))   { m_yagBasinci = v;   emit yagBasinciChanged(); } }
void EngineModel::setYagSicakligi(double v) { if (!qFuzzyCompare(m_yagSicakligi + 1.0, v + 1.0)) { m_yagSicakligi = v; emit yagSicakligiChanged(); } }
void EngineModel::setTitresim(double v)     { if (!qFuzzyCompare(m_titresim + 1.0, v + 1.0))     { m_titresim = v;     emit titresimChanged(); } }
