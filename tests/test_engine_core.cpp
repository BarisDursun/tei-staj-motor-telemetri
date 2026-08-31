// engine_core.h icin Qt'siz saf C++ testler.
// Amac: engine_core.h'nin gercekten Qt'den bagimsiz oldugunu kanitlamak (NFR-05,
// bkz. docs/gereksinimler.md) - bu yuzden burada Qt5::Test yerine kucuk, elle
// yazilmis bir CHECK makrosu kullaniliyor.
#include "../engine_core.h"
#include <cstdio>
#include <cmath>

static int g_total = 0;
static int g_fail = 0;

//bool gibi kesin değerler için
#define CHECK(cond) do { \
    ++g_total; \
    if (!(cond)) { ++g_fail; std::printf("  FAIL  L%d: %s\n", __LINE__, #cond); } \
} while (0)

//hesaplanmış bir float/double değeri bekleneni ile karşılaştırmak için
#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(double(a) - double(b)) < (eps))

namespace {

// Baseline: her parametre alarm esiklerinin guvenli bolgesinde - boylece tek bir
// parametreyi degistirip digerlerinin alarma karismadigindan emin olabiliriz.
TF10000 safeTF() {
    TF10000 e(0.0f);
    e.param_Devir1 = 50.0f; e.param_Devir2 = 50.0f; e.param_Basinc = 10.0f;
    e.param_EGT = 600.0f; e.param_Yakit = 1000.0f; e.param_YagBasinci = 50.0f;
    e.param_YagSicakligi = 80.0f; e.param_Titresim = 2.0f;
    return e;
}

PD170 safePD() {
    PD170 e(0.0f);
    e.param_Devir1 = 2000.0f; e.param_Devir2 = 90.0f; e.param_Basinc = 2.0f;
    e.param_EGT = 400.0f; e.param_Yakit = 15.0f; e.param_YagBasinci = 3.5f;
    e.param_YagSicakligi = 90.0f; e.param_Titresim = 1.5f;
    return e;
}

// ---------- A. WearFactor / yas sinirlari ----------
// FR-09, NFR-04: yasa gore yiprama faktorunun (0, negatif, cok buyuk yas) dogru hesaplandigini dogrular.
void test_wearFactor() {
    std::printf("A. WearFactor / yas sinirlari\n");

    CHECK_NEAR(TF10000(0.0f).WearFactor(), 0.0, 1e-6); // taze motor: yiprama sifir

    // NFR-04: negatif yas 0'a sabitlenmeli, fiziksel olmayan (negatif) w uretmemeli.
    CHECK_NEAR(TF10000(-50.0f).WearFactor(), 0.0, 1e-6);

    // Cok buyuk yas: uzel doygunluk egrisi 1'e yaklasir ama asla esitlenmez (float hassasiyeti icin age=100 secildi).
    const float wOld = TF10000(100.0f).WearFactor();
    CHECK(wOld < 1.0f);
    CHECK(wOld > 0.9999f);
}

// ---------- B. GetMaintenanceStatus uc seviye esigi ----------
// FR-17: bakim durumunun (SAGLIKLI/IZLENMELI/BAKIM GEREKLI) w=0.45 ve w=0.70 sinirlarinda dogru degistigini dogrular.
void test_maintenanceStatus() {
    std::printf("B. GetMaintenanceStatus uc seviye esigi (w=0.45 / w=0.70 sinirlari)\n");

    CHECK(TF10000(0.0f).GetMaintenanceStatus() == MaintenanceStatus::Healthy);
    CHECK(TF10000(4.7f).GetMaintenanceStatus() == MaintenanceStatus::Healthy);              // w~0.444 < 0.45
    CHECK(TF10000(4.9f).GetMaintenanceStatus() == MaintenanceStatus::Watch);                // w~0.458 >= 0.45
    CHECK(TF10000(9.5f).GetMaintenanceStatus() == MaintenanceStatus::Watch);                // w~0.695 < 0.70
    CHECK(TF10000(9.8f).GetMaintenanceStatus() == MaintenanceStatus::MaintenanceRequired);  // w~0.706 >= 0.70
}

// ---------- C. Engine_Start guc clamp ----------
// FR-24, NFR-04: Engine_Start'a gelen gecersiz (negatif/asiri) guc degerinin rolanti/tam-guce clamp edildigini dogrular.
void test_powerClamp() {
    std::printf("C. Engine_Start guc clamp (negatif/asiri guc rolanti/tam-guc gibi davranmali)\n");

    TF10000 negPower(0.0f);   negPower.Engine_Start(-5.0f, 25.0f);
    TF10000 idle(0.0f);       idle.Engine_Start(0.0f, 25.0f);
    CHECK_NEAR(negPower.param_Devir1, idle.param_Devir1, 1e-4);
    CHECK_NEAR(negPower.param_EGT,    idle.param_EGT,    1e-4);

    TF10000 overPower(0.0f);  overPower.Engine_Start(50.0f, 25.0f);
    TF10000 full(0.0f);       full.Engine_Start(1.0f, 25.0f);
    CHECK_NEAR(overPower.param_Devir1, full.param_Devir1, 1e-4);
    CHECK_NEAR(overPower.param_EGT,    full.param_EGT,    1e-4);
}

// ---------- D. ParamCeilings tutarliligi ----------
// FR-08 (destekleyici): jitter clamp'inin dayandigi ParamCeilings'in taze motorun tam guc degeriyle birebir tutarli oldugunu dogrular.
void test_paramCeilingsMatchNominal() {
    std::printf("D. Taze motorda (age=0) tam guc degeri, ParamCeilings ile birebir esit olmali\n");

    TF10000 tf(0.0f); tf.Engine_Start(1.0f, 25.0f); // sensorTemp=25 (referans) -> EGT_SENSOR_GAIN katkisi sifir
    const ParamCeilings c = tf.GetParamCeilings();
    CHECK_NEAR(tf.param_Devir1, c.devir1, 1e-3);
    CHECK_NEAR(tf.param_EGT,    c.egt,    1e-3);

    PD170 pd(0.0f); pd.Engine_Start(1.0f, 25.0f);
    const ParamCeilings cp = pd.GetParamCeilings();
    CHECK_NEAR(pd.param_Devir1, cp.devir1, 1e-3);
    CHECK_NEAR(pd.param_EGT,    cp.egt,    1e-3);
}

// ---------- E. TF10000 EvaluateAlarm sinir matrisi (bandHigh, ">=" semantigi) ----------
// FR-12, FR-14a: TF10000'de her parametrenin warnAt/critAt esiginde dogru alarm seviyesi urettigini dogrular.
void test_tf10000_alarmBoundaries() {
    std::printf("E. TF10000 EvaluateAlarm sinir matrisi\n");

    // warnAt/critAt tam esikte "dahil" (bandHigh ">=" kullaniyor) - dort nokta test edilir:
    // esigin hemen alti (Normal), tam esik (Warning/Critical), sonraki esigin hemen alti (Warning).
    auto check = [](float warnAt, float critAt, auto setter) {
        { TF10000 e = safeTF(); setter(e, warnAt - 0.01f); CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Normal); }
        { TF10000 e = safeTF(); setter(e, warnAt);         CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
        { TF10000 e = safeTF(); setter(e, critAt - 0.01f); CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
        { TF10000 e = safeTF(); setter(e, critAt);         CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical); }
    };

    check(95.0f,  105.0f, [](TF10000 &e, float v) { e.param_Devir1 = v; });
    check(95.0f,  105.0f, [](TF10000 &e, float v) { e.param_Devir2 = v; });
    check(4.0f,   5.0f,   [](TF10000 &e, float v) { e.param_Titresim = v; });
    check(775.0f, 850.0f, [](TF10000 &e, float v) { e.param_EGT = v; });
    check(100.0f, 110.0f, [](TF10000 &e, float v) { e.param_YagSicakligi = v; }); // FR-14a: yeni eklenen alarm
}

// FR-12: TF10000 YagBasinci'nin iki yonlu (dusuk VE yuksek) bandRange alarm mantigini dogrular.
void test_tf10000_yagBasinciBandRange() {
    std::printf("E2. TF10000 YagBasinci bandRange (dusuk taraf + yuksek taraf)\n");

    // dusuk taraf: warnLow=35, critLow=30 (deger dustukce tehlikeli)
    { TF10000 e = safeTF(); e.param_YagBasinci = 35.01f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Normal); }
    { TF10000 e = safeTF(); e.param_YagBasinci = 35.0f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { TF10000 e = safeTF(); e.param_YagBasinci = 30.01f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { TF10000 e = safeTF(); e.param_YagBasinci = 30.0f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical); }

    // yuksek taraf: warnHigh=75, critHigh=80
    { TF10000 e = safeTF(); e.param_YagBasinci = 74.99f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Normal); }
    { TF10000 e = safeTF(); e.param_YagBasinci = 75.0f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { TF10000 e = safeTF(); e.param_YagBasinci = 79.99f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { TF10000 e = safeTF(); e.param_YagBasinci = 80.0f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical); }
}

// ---------- F. PD170 EvaluateAlarm sinir matrisi ----------
// FR-12, FR-14b: PD170'de her parametrenin warnAt/critAt esiginde dogru alarm seviyesi urettigini dogrular.
void test_pd170_alarmBoundaries() {
    std::printf("F. PD170 EvaluateAlarm sinir matrisi\n");

    auto check = [](float warnAt, float critAt, auto setter) {
        { PD170 e = safePD(); setter(e, warnAt - 0.01f); CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Normal); }
        { PD170 e = safePD(); setter(e, warnAt);         CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
        { PD170 e = safePD(); setter(e, critAt - 0.01f); CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
        { PD170 e = safePD(); setter(e, critAt);         CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical); }
    };

    check(2500.0f, 2800.0f, [](PD170 &e, float v) { e.param_Devir1 = v; });
    check(100.0f,  120.0f,  [](PD170 &e, float v) { e.param_Devir2 = v; }); // sogutma suyu sicakligi (RPM degil)
    check(2.2f,    2.7f,    [](PD170 &e, float v) { e.param_Titresim = v; });
    check(480.0f,  550.0f,  [](PD170 &e, float v) { e.param_EGT = v; });
    check(2.3f,    2.5f,    [](PD170 &e, float v) { e.param_Basinc = v; }); // FR-14b: yeni eklenen overboost alarmi
}

// FR-12: PD170 YagBasinci'nin iki yonlu (dusuk VE yuksek) bandRange alarm mantigini dogrular.
void test_pd170_yagBasinciBandRange() {
    std::printf("F2. PD170 YagBasinci bandRange (dusuk taraf + yuksek taraf)\n");

    // dusuk taraf: warnLow=2.0, critLow=1.5
    { PD170 e = safePD(); e.param_YagBasinci = 2.01f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Normal); }
    { PD170 e = safePD(); e.param_YagBasinci = 2.0f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { PD170 e = safePD(); e.param_YagBasinci = 1.51f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { PD170 e = safePD(); e.param_YagBasinci = 1.5f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical); }

    // yuksek taraf: warnHigh=5.8, critHigh=6.0
    { PD170 e = safePD(); e.param_YagBasinci = 5.79f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Normal); }
    { PD170 e = safePD(); e.param_YagBasinci = 5.8f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { PD170 e = safePD(); e.param_YagBasinci = 5.99f; CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Warning); }
    { PD170 e = safePD(); e.param_YagBasinci = 6.0f;  CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical); }
}

// ---------- G. worstOf: birden fazla esik asimi ----------
// FR-14: birden fazla parametre ayni anda esik asarsa gosterilen sonucun en kotusu oldugunu dogrular.
void test_worstOfMultipleAlarms() {
    std::printf("G. Birden fazla parametre esik asarsa en kotusu gosterilmeli (worstOf)\n");

    TF10000 e = safeTF();
    e.param_Devir1 = 95.0f;  // Warning
    e.param_EGT    = 850.0f; // Critical
    CHECK(e.EvaluateAlarm() == EngineAlarmLevel::Critical);
}

// ---------- H. GetWearDeviations, WearScale ile guce gore olcekleniyor (FR-09) ----------
// FR-09: yiprama etkisinin (WearScale) rolantide az, tam guçte fazla oldugunu dogrular.
void test_wearScalesWithPower() {
    std::printf("H. Yiprama etkisi rolantide az, tam guçte fazla olmali (WearScale)\n");

    TF10000 agedTF(8.0f); // w~0.632, gozle gorulur asinma
    const WearDeviations idleTF = agedTF.GetWearDeviations(0.0f);
    const WearDeviations fullTF = agedTF.GetWearDeviations(1.0f);
    CHECK(std::fabs(fullTF.egtPct)      > std::fabs(idleTF.egtPct));
    CHECK(std::fabs(fullTF.titresimPct) > std::fabs(idleTF.titresimPct));

    PD170 agedPD(8.0f);
    const WearDeviations idlePD = agedPD.GetWearDeviations(0.0f);
    const WearDeviations fullPD = agedPD.GetWearDeviations(1.0f);
    CHECK(std::fabs(fullPD.egtPct) > std::fabs(idlePD.egtPct));
}

// ---------- I. STM sensör sıcaklığı, EGT'ye motora özgü oranla yansımalı ----------
// FR-09a: STM sensor sapmasinin, EGT_SENSOR_GAIN ile her motorun kendi EGT araliginin AYNI yuzdesini kaydirdigini dogrular.
void test_sensorTempScalesEgtProportionallyPerEngine() {
    std::printf("I. sensorTemp, EGT'ye EGT_SENSOR_GAIN ile motora ozgu oranlanarak yansimali\n");

    TF10000 tf(0.0f);
    tf.Engine_Start(0.0f, 25.0f); // referans noktasi: sapma sifir
    const float tfBaseline = tf.param_EGT;
    tf.Engine_Start(0.0f, 35.0f); // +10 derece sensor sapmasi
    CHECK_NEAR(tf.param_EGT - tfBaseline, 10.0 * TF10000::EGT_SENSOR_GAIN, 0.05);

    PD170 pd(0.0f);
    pd.Engine_Start(0.0f, 25.0f);
    const float pdBaseline = pd.param_EGT;
    pd.Engine_Start(0.0f, 35.0f);
    CHECK_NEAR(pd.param_EGT - pdBaseline, 10.0 * PD170::EGT_SENSOR_GAIN, 0.05);

    // Asil nokta: ayni +10 derecelik sensor sapmasi, iki motorun da kendi EGT
    // araliginin (span) AYNI yuzdesini kaydiriyor - "motora gore oranli" budur.
    const double tfSpan = 470.0; // TF10000: 480 rolanti -> 950 tam guc
    const double pdSpan = 350.0; // PD170:   250 rolanti -> 600 tam guc
    const double tfShiftPct = (tf.param_EGT - tfBaseline) / tfSpan;
    const double pdShiftPct = (pd.param_EGT - pdBaseline) / pdSpan;
    CHECK_NEAR(tfShiftPct, pdShiftPct, 1e-6);
}

} // namespace

int main() {
    test_wearFactor();
    test_maintenanceStatus();
    test_powerClamp();
    test_paramCeilingsMatchNominal();
    test_tf10000_alarmBoundaries();
    test_tf10000_yagBasinciBandRange();
    test_pd170_alarmBoundaries();
    test_pd170_yagBasinciBandRange();
    test_worstOfMultipleAlarms();
    test_wearScalesWithPower();
    test_sensorTempScalesEgtProportionallyPerEngine();

    std::printf("\n%d/%d test PASS\n", g_total - g_fail, g_total);
    return g_fail == 0 ? 0 : 1;
}
