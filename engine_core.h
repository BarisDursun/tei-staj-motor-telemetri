#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include <string>
#include <cmath>

// Qt'ye bagimli olmasin diye (Engine sinifi framework-agnostik kalsin) ayri
// bir enum - EngineModel bunu kendi Q_ENUM'lu AlarmLevel'ina cevirir.
enum class EngineAlarmLevel { Normal, Warning, Critical };

// Uzun vadeli asinma durumu - EngineAlarmLevel'dan FARKLI bir kavram: alarm
// aninlik calisma guvenligini (su an cok mu sicak/hizli), bu ise motorun
// yillar/saatler icindeki genel yipranmisligini gosterir. Yeni bir motor
// asiri yuklenirse alarm CRITICAL olabilir ama bakim durumu yine HEALTHY'dir;
// eski bir motor rolantide bile MAINTENANCE_REQUIRED gosterebilir.
enum class MaintenanceStatus { Healthy, Watch, MaintenanceRequired };

// Her parametre icin ayri yukselis (rise) ve dusus (fall) zaman sabiti (saniye,
// ~%95 hedefe yaklasma suresi). Ikisi FARKLI cunku gercek motorlarda da farkli:
// - Guc/isi ARTARKEN aktif bir surec (yanma, pompa basinci) tahrik eder, hizli.
// - Guc/isi AZALIRKEN sadece pasif atalet/sogumaya kaliyor, cok daha yavas.
// Kaynak: gercek turbofan spool-up ~6sn (CFM56-3, idle->tam guc), spool-down
// ~60-120sn (buyuk turbofanlarda "1/x" egrisi, kalinti donus) - PPRuNe/Key Aero
// forum tartismalari. Yag/govde sicakligi gercekte ortam sicakligina donmesi
// saatler surer (~8 saat), operasyonel sogutma penceresi 3-5 dakika - burada
// o 3-5 dakikalik pencere baz alindi (tam saatlik sogumaya kadar beklemek
// interaktif bir demo icin anlamsiz olurdu).
//
// Hangi parametrenin "hizli/mekanik" hangisinin "yavas/termal" oldugu motor
// tipine gore degisir (ornegin PD170'te Devir2 aslinda sogutma suyu SICAKLIGI,
// RPM degil) - bu yuzden profil EngineModel'de degil, her Engine alt sinifinda
// kendi motoruna gore tanimlaniyor.
struct SpoolProfile {
    double devir1Rise,       devir1Fall;
    double devir2Rise,       devir2Fall;
    double basincRise,       basincFall;
    double egtRise,          egtFall;
    double yakitRise,        yakitFall;
    double yagBasinciRise,   yagBasinciFall;
    double yagSicakligiRise, yagSicakligiFall;
    double titresimRise,     titresimFall;
};

// Her parametrenin bu motorda gercekten ulasabilecegi tavan degeri (yani
// Engine_Start(1.0, ...) sonucu). EngineModel, gurultu (jitter) uygulandiktan
// sonra degerleri bu tavanla sinirlar - aksi halde orn. "%100.2" gibi
// fiziksel olarak anlamsiz bir okuma gorunebilir.
struct ParamCeilings {
    double devir1, devir2, basinc, egt, yakit, yagBasinci, yagSicakligi, titresim;
};

// Temel Motor Sınıfı
class Engine {
public:
    std::string engineName;
    float powerLevel;
    float param_Devir1;
    float param_Devir2;
    float param_Basinc;
    float param_EGT;
    float param_Yakit;
    float param_YagBasinci;
    float param_YagSicakligi;
    float param_Titresim;
    bool alarmState;

    // Motorun yasi (yil) - filo simulasyonu icin. Yeni motor icin 0.
    float ageYears = 0.0f;

    Engine(std::string name) { engineName = name; alarmState = false; }
    virtual ~Engine() {}
    virtual void Engine_Start(float power, float lm35Temp) = 0;

    // 0 (sifir km) ile ~1 (agir yipranmis) arasinda doyum egrisi (saturating
    // curve) - dogrusal degil, cunku gercek motorlarda "bozulma hizi baslangicta
    // en yuksek, sonra bir platoya oturuyor" (kaynak: EGT margin deterioration
    // arastirmasi, bkz. GetMaintenanceStatus yorumu). tau=8 -> ~5 yilda %46,
    // ~10 yilda %71, ~20 yilda %92 asinma.
    float WearFactor() const {
        const float tau = 8.0f;
        return 1.0f - std::exp(-ageYears / tau);
    }

    // Otomatik bakim karari - alarm seviyesinden BAGIMSIZ, sadece yasa/asinmaya
    // dayanir. Esikler (0.45 / 0.70) WearFactor'un dogal dagilimina gore
    // secildi (bkz. yorumu) - ~5 yil "izlenmeli", ~10 yil "bakim gerekli"ye denk gelir.
    MaintenanceStatus GetMaintenanceStatus() const {
        const float w = WearFactor();
        if (w >= 0.70f) return MaintenanceStatus::MaintenanceRequired;
        if (w >= 0.45f) return MaintenanceStatus::Watch;
        return MaintenanceStatus::Healthy;
    }

    // Motora ozgu esiklere gore genel alarm seviyesini hesaplar (tum
    // parametrelerin en kotusu). Esikler temsili/demo degerleridir - gercek
    // TEI referans verisi elde edilirse guncellenmeli (bkz. genisletme-plani.md).
    virtual EngineAlarmLevel EvaluateAlarm() const = 0;

    // Motora ozgu spool-up/spool-down zaman sabitleri (bkz. SpoolProfile yorumu).
    virtual SpoolProfile GetSpoolProfile() const = 0;

    // Motora ozgu, %100 guctekiyle birebir ayni tavan degerleri (bkz. ParamCeilings yorumu).
    virtual ParamCeilings GetParamCeilings() const = 0;

protected:
    static EngineAlarmLevel worstOf(EngineAlarmLevel a, EngineAlarmLevel b) {
        return static_cast<int>(b) > static_cast<int>(a) ? b : a;
    }
    // Sadece ust sinira gore (deger arttikca kotulesen parametreler icin: EGT, titresim, devir).
    static EngineAlarmLevel bandHigh(float value, float warnAt, float critAt) {
        if (value >= critAt) return EngineAlarmLevel::Critical;
        if (value >= warnAt) return EngineAlarmLevel::Warning;
        return EngineAlarmLevel::Normal;
    }
    // Hem cok dusuk hem cok yuksek deger kotu olan parametreler icin (yag basinci gibi).
    static EngineAlarmLevel bandRange(float value, float warnLow, float critLow, float warnHigh, float critHigh) {
        if (value <= critLow || value >= critHigh) return EngineAlarmLevel::Critical;
        if (value <= warnLow || value >= warnHigh) return EngineAlarmLevel::Warning;
        return EngineAlarmLevel::Normal;
    }
};

// TF10000 Sınıfı
class TF10000 : public Engine {
public:
    explicit TF10000(float ageYears = 0.0f) : Engine("TF10000") { this->ageYears = ageYears; }

    // Asinma carpanlari (WEAR_* sabitleri) gercek motor kondisyon izleme
    // (ECTM) literaturunden: EGT ayni guc ayarinda YUKSELIR (EGT marji daralir -
    // kompresor kirlenmesi, turbin ucu boslugu artisi), yakit tuketimi verim
    // kaybiyla YUKSELIR, yag basinci yatak (bearing) boslugu buyudukce DUSER,
    // titresim yatak asinmasi/dengesizlikle en belirgin sekilde YUKSELIR.
    // Bu sabitler hem Engine_Start'ta hem GetParamCeilings'te AYNI sekilde
    // kullaniliyor - biri degisirse digeri de guncellenmeli.
    static constexpr float WEAR_EGT = 0.18f;
    static constexpr float WEAR_YAKIT = 0.12f;
    static constexpr float WEAR_YAG_BASINCI = -0.30f;
    static constexpr float WEAR_YAG_SICAKLIGI = 0.15f;
    static constexpr float WEAR_TITRESIM = 1.5f;

    void Engine_Start(float power, float lm35Temp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;
        const float w = WearFactor();

        param_Devir1 = 35.0f + (power * 65.0f);
        param_Devir2 = 65.0f + (power * 35.0f);
        param_Basinc = 2.0f  + (power * 18.0f);
        param_EGT    = (480.0f + (power * 470.0f) + (lm35Temp - 25.0f)) * (1.0f + w * WEAR_EGT);
        param_Yakit  = (250.0f + (power * 2850.0f)) * (1.0f + w * WEAR_YAKIT);
        param_YagBasinci   = (40.0f + (power * 30.0f)) * (1.0f + w * WEAR_YAG_BASINCI);
        param_YagSicakligi = (60.0f + (power * 60.0f)) * (1.0f + w * WEAR_YAG_SICAKLIGI);
        param_Titresim     = (1.5f  + (power * 4.5f)) * (1.0f + w * WEAR_TITRESIM);
    }

    EngineAlarmLevel EvaluateAlarm() const override {
        EngineAlarmLevel level = EngineAlarmLevel::Normal;
        level = worstOf(level, bandHigh(param_Devir1, 95.0f, 105.0f));
        level = worstOf(level, bandHigh(param_Devir2, 95.0f, 105.0f));
        level = worstOf(level, bandRange(param_YagBasinci, 35.0f, 30.0f, 75.0f, 80.0f));
        level = worstOf(level, bandHigh(param_Titresim, 4.0f, 5.0f));
        level = worstOf(level, bandHigh(param_EGT, 775.0f, 850.0f));
        return level;
    }

    SpoolProfile GetSpoolProfile() const override {
        return SpoolProfile{
            /* devir1 (N1 fan): buyuk kutle - CFM56 gibi kucuk turbofanlarda
               idle->tam guc ~6-10sn, ama duruste "windmill" ile durmasi
               (aktif fren yok) cok daha yavas, ~1-2 dakika. */
            /*devir1Rise*/       10.0, /*devir1Fall*/       90.0,
            /* devir2 (N2 core): fandan daha kucuk kutle, daha hizli tepki verir. */
            /*devir2Rise*/       5.0,  /*devir2Fall*/        60.0,
            /* basinc: kompresor basinci dogrudan N2'ye bagli, onunla birlikte degisir. */
            /*basincRise*/       5.0,  /*basincFall*/        60.0,
            /* egt: yanma gazi sicakligi - yakit kesilince nispeten hizli duser,
               ama govde termal soak'i yuzunden saf mekanikten biraz daha yavas. */
            /*egtRise*/          8.0,  /*egtFall*/           40.0,
            /* yakit: valf - acilmasi/kapanmasi neredeyse anlik. */
            /*yakitRise*/        1.5,  /*yakitFall*/         0.5,
            /* yag basinci: pompa N2'ye bagli, RPM ile birlikte duser. */
            /*yagBasinciRise*/   5.0,  /*yagBasinciFall*/    60.0,
            /* yag sicakligi: buyuk sivi kutlesi - gercekte ortam sicakligina
               donmesi saatler surer, burada operasyonel sogutma penceresi
               (3-5 dk) baz alindi. */
            /*yagSicakligiRise*/ 60.0, /*yagSicakligiFall*/  240.0,
            /* titresim: esas olarak fan donusune bagli, onunla birlikte degisir. */
            /*titresimRise*/     10.0, /*titresimFall*/      90.0
        };
    }

    // Engine_Start(1.0, 25.0)'un urettigi degerlerle birebir ayni - asinma
    // carpanlari dahil (yoksa eski bir motorun yukselen EGT'si burada
    // yanlislikla "yeni motor tavani"na kirpilirdi, asinma etkisi gizlenirdi).
    ParamCeilings GetParamCeilings() const override {
        const float w = WearFactor();
        return ParamCeilings{
            /*devir1*/ 100.0, /*devir2*/ 100.0, /*basinc*/ 20.0,
            /*egt*/ 950.0 * (1.0 + w * WEAR_EGT),
            /*yakit*/ 3100.0 * (1.0 + w * WEAR_YAKIT),
            /*yagBasinci*/ 70.0 * (1.0 + w * WEAR_YAG_BASINCI),
            /*yagSicakligi*/ 120.0 * (1.0 + w * WEAR_YAG_SICAKLIGI),
            /*titresim*/ 6.0 * (1.0 + w * WEAR_TITRESIM)
        };
    }
};

// PD170 Sınıfı
class PD170 : public Engine {
public:
    PD170() : Engine("PD170") {}
    void Engine_Start(float power, float lm35Temp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;

        param_Devir1 = 1000.0f + (power * 1800.0f);
        param_Devir2 = 70.0f + (power * 35.0f);
        param_Basinc = 1.0f  + (power * 1.6f);
        param_EGT    = 250.0f + (power * 350.0f) + (lm35Temp - 25.0f);
        param_Yakit  = 4.0f  + (power * 32.0f);
        param_YagBasinci   = 2.5f  + (power * 3.0f);
        param_YagSicakligi = 70.0f + (power * 45.0f);
        param_Titresim     = 1.0f  + (power * 2.0f);
    }

    EngineAlarmLevel EvaluateAlarm() const override {
        EngineAlarmLevel level = EngineAlarmLevel::Normal;
        level = worstOf(level, bandHigh(param_Devir1, 2500.0f, 2800.0f));
        level = worstOf(level, bandHigh(param_Devir2, 100.0f, 120.0f));
        level = worstOf(level, bandRange(param_YagBasinci, 2.0f, 1.5f, 5.8f, 6.0f));
        level = worstOf(level, bandHigh(param_Titresim, 2.2f, 2.7f));
        level = worstOf(level, bandHigh(param_EGT, 480.0f, 550.0f));
        return level;
    }

    SpoolProfile GetSpoolProfile() const override {
        return SpoolProfile{
            /* devir1 (krank mili RPM): pistonlu motor, turbin fanina gore
               kutlesi/atalet orani cok daha kucuk + kompresyon frenlemesi var,
               yakit kesilince nispeten hizli duser (turbine gore cok daha hizli). */
            /*devir1Rise*/       3.0,  /*devir1Fall*/        5.0,
            /* DIKKAT: Devir2 bu motorda RPM DEGIL, sogutma suyu SICAKLIGI -
               buyuk sivi kutlesi, termal grupta olmali (mekanik degil). */
            /*devir2Rise*/       90.0, /*devir2Fall*/        200.0,
            /* basinc: manifold MAP, kelebek klapesinin konumuna neredeyse anlik tepki verir. */
            /*basincRise*/       1.0,  /*basincFall*/        1.0,
            /* egt: egzoz gazi sicakligi - piston motorda turbine gore daha
               kucuk hacimli egzoz sistemi, biraz daha hizli soner. */
            /*egtRise*/          5.0,  /*egtFall*/           15.0,
            /* yakit: enjeksiyon pompasi - neredeyse anlik kesilir. */
            /*yakitRise*/        1.0,  /*yakitFall*/         0.5,
            /* yag basinci: pompa krank miline bagli, RPM ile birebir duser. */
            /*yagBasinciRise*/   3.0,  /*yagBasinciFall*/    5.0,
            /* yag sicakligi: buyuk sivi kutlesi - TF10000'deki gibi cok yavas. */
            /*yagSicakligiRise*/ 80.0, /*yagSicakligiFall*/  220.0,
            /* titresim: piston atesleme titresimi, dogrudan RPM'e bagli. */
            /*titresimRise*/     3.0,  /*titresimFall*/      5.0
        };
    }

    // Engine_Start(1.0, 25.0)'un urettigi degerlerle birebir ayni.
    ParamCeilings GetParamCeilings() const override {
        return ParamCeilings{
            /*devir1*/ 2800.0, /*devir2*/ 105.0, /*basinc*/ 2.6, /*egt*/ 600.0,
            /*yakit*/ 36.0, /*yagBasinci*/ 5.5, /*yagSicakligi*/ 115.0, /*titresim*/ 3.0
        };
    }
};

#endif // ENGINE_CORE_H
