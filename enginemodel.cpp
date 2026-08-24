#include "enginemodel.h"
#include <QTimer>
#include <QRandomGenerator>
#include <QVariantMap>
#include <QVector>
#include <algorithm>

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

// WearDeviations'i, mutlak sapmasi esigi (%10) asan parametreleri en buyuk
// sapma basta olacak sekilde okunabilir cumlelere cevirir - "bakim neden
// gerekli" sorusuna somut cevap vermek icin (bkz. maintenanceStatusText).
QVariantList buildWearNotes(const WearDeviations &d) {
    struct Item { double pct; QString label; };
    QVector<Item> items = {
        {d.titresimPct,     QStringLiteral("Titreşim")},
        {d.yagBasinciPct,   QStringLiteral("Yağ Basıncı")},
        {d.egtPct,          QStringLiteral("EGT")},
        {d.yagSicakligiPct, QStringLiteral("Yağ Sıcaklığı")},
        {d.yakitPct,        QStringLiteral("Yakıt Akışı")},
    };
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
    // Kasitli olarak SADECE kimlik/yas bilgisi (bir filo kayit defterinde
    // gorulebilecek turden) - bakim durumu burada YOK. Onceden gosterilirse
    // operator hicbir motoru test etmeden hepsinin cevabini gormus olurdu,
    // bu da "once test et, sonra karar ver" akisini anlamsizlastirirdi.
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
    // Yeni motor bu oturumda henuz test edilmedi - teshis QML'e kapali kalir,
    // gercekten calistirilip oturana kadar (bkz. simulationTick).
    m_tested = false;

    if (newEngine) {
        newEngine->Engine_Start(0.0, 25.0);
        refreshFromEngine(/*withJitter=*/false);

        // Gercek teshis burada hesaplanip saklanir ama m_tested false oldugu
        // surece disariya (maintenanceStatusText/wearNotes getter'lari
        // uzerinden) acilmaz - motorun "gercek" durumu var olsa da, operator
        // onu test etmeden gormemis sayilir. Sapma yuzdeleri (0 guctekiyle)
        // motor calisirken her tikte guncellenmeye devam eder (bkz. simulationTick).
        m_maintenanceStatusText = maintenanceStatusToText(newEngine->GetMaintenanceStatus());
        m_wearNotes = buildWearNotes(newEngine->GetWearDeviations(0.0f));
        emit maintenanceStatusChanged();
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

    // Motor gercekten calisip oturunca (ayni esik: alarm degerlendirmesinin
    // acildigi nokta) bu oturum icin "test edildi" sayilir - bakim teshisi
    // artik QML'e acilir. Bir kere acildiktan sonra kapanmaz (spool-down'da
    // bile son test sonucunu gostermeye devam eder, motor degistirilene kadar).
    if (m_running && m_factorYagBasinci >= 0.97 && !m_tested) {
        m_tested = true;
        emit maintenanceStatusChanged();
    }

    // Test edildikten sonra sapma notlari GUNCEL guce gore surekli yenilenir -
    // gaz kolu hareket ettikce yuzdeler de degisir (rolantide zayif, tam
    // guçte belirgin - bkz. TF10000::WearScale). maintenanceStatusText
    // (SAGLIKLI/IZLENMELI/BAKIM GEREKLI) kasitli olarak degismez, o motorun
    // sabit yapisal durumunu yansitir.
    if (m_tested) {
        const QVariantList freshNotes = buildWearNotes(currentEngine->GetWearDeviations(static_cast<float>(m_actualPower / 100.0)));
        if (freshNotes != m_wearNotes) {
            m_wearNotes = freshNotes;
            emit maintenanceStatusChanged();
        }
    }
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

    // Alarm degerlendirmesi SADECE yag basinci spool'u neredeyse tamamen
    // bittiyse (%97+) yapilir. Onemli detay: gosterilen deger = hedef * faktor,
    // yani faktor 0'dan 1'e dogrusal tirmanirken deger de ZORUNLU olarak
    // "dusuk basinc" bandindan geciyor (0'dan baslayip yukari cikan her
    // dogru, aradaki her degerden geçer) - dusuk bir esik (orn. %60) bu
    // geciste hala yakalanirdi, cunku o anda gosterilen deger de hedefin
    // sadece %60'i. Gercek ucaklarda da ayni sorun "start inhibit" mantigiyla
    // cozulur: motor ilk calisirken belirli alarmlar pompa/basinc gercekten
    // oturana kadar bilerek bastirilir. m_running=false oldugunda zaten
    // spool geriye gidip bu esigin altina duser, ayrica kontrole gerek yok.
    if (m_factorYagBasinci >= 0.97) {
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
