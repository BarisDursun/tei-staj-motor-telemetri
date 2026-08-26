#ifndef ENGINE_CORE_H                    //motorun fizik modelini tutar
#define ENGINE_CORE_H

#include <string>
#include <cmath>


enum class EngineAlarmLevel { Normal, Warning, Critical }; // Motorun anlık tehlike durumunu belirten enum.
enum class MaintenanceStatus { Healthy, Watch, MaintenanceRequired };// Motorun uzun vadeli yıpranma (kondisyon) durumu.


// Motor parametrelerinin rise & fail sürelerini tutar.-- Termal atalet & mekanik atalet süreleri farklıdır.
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

// Motorun %100 güçte ulaşabileceği maksimum fiziksel tavan değerleri. Gürültü eklendiğinde>100 olmasın diye
struct ParamCeilings {
    double devir1, devir2, basinc, egt, yakit, yagBasinci, yagSicakligi, titresim;
};

// Yıpranan parametrelerin sıfır motor referansından yüzde kaç saptığını tutar.
struct WearDeviations {
    double egtPct, yakitPct, yagBasinciPct, yagSicakligiPct, titresimPct;
};

// Base Motor Sınıfı-Polimorfizm-
class Engine {
public:
    std::string engineName;
    float powerLevel;
    float param_Devir1, param_Devir2, param_Basinc, param_EGT;
    float param_Yakit, param_YagBasinci, param_YagSicakligi, param_Titresim;
    bool alarmState;
    float ageYears = 0.0f; // Motorun yaşı (Yıpranma simülasyonu için)

    Engine(std::string name) { engineName = name; alarmState = false; }
    virtual ~Engine() {}

    // Alt sınıfların (TF10000, PD170) kendine has motor tepkilerini yazacağı saf sanal fonksiyon.
    virtual void Engine_Start(float power, float lm35Temp) = 0;


    float WearFactor() const {      // Motorun yaşa göre eponantional yıpranma faktörü hesaplar 0-1 arası
        const float tau = 8.0f;
        return 1.0f - std::exp(-ageYears / tau);
    }

    MaintenanceStatus GetMaintenanceStatus() const {    // Yıpranmaya göre  bakım durumunu belirler enumda
        const float w = WearFactor();
        if (w >= 0.70f) return MaintenanceStatus::MaintenanceRequired;
        if (w >= 0.45f) return MaintenanceStatus::Watch;
        return MaintenanceStatus::Healthy;
    }

    // Polimorfik fonksiyonlar: Her motor için ayrı
    virtual EngineAlarmLevel EvaluateAlarm() const = 0;
    virtual SpoolProfile GetSpoolProfile() const = 0;
    virtual ParamCeilings GetParamCeilings() const = 0;
    virtual WearDeviations GetWearDeviations(float power) const = 0;

protected:
    // İki alarmdan worst olanı seçer
    static EngineAlarmLevel worstOf(EngineAlarmLevel a, EngineAlarmLevel b) {
        return static_cast<int>(b) > static_cast<int>(a) ? b : a;
    }
    // Değer arttıkça tehlikeli olan parametreler için alarm check - Titreşim, EGT-
    static EngineAlarmLevel bandHigh(float value, float warnAt, float critAt) {
        if (value >= critAt) return EngineAlarmLevel::Critical;
        if (value >= warnAt) return EngineAlarmLevel::Warning;
        return EngineAlarmLevel::Normal;
    }
    // <> alarm check fot extreme value   - Yağ Basıncı-
    static EngineAlarmLevel bandRange(float value, float warnLow, float critLow, float warnHigh, float critHigh) {
        if (value <= critLow || value >= critHigh) return EngineAlarmLevel::Critical;
        if (value <= warnLow || value >= warnHigh) return EngineAlarmLevel::Warning;
        return EngineAlarmLevel::Normal;
    }
};






// Turbofan Motor Sınıfı -inheritance-
class TF10000 : public Engine {
public:
    explicit TF10000(float ageYears = 0.0f) : Engine("TF10000") { this->ageYears = ageYears; } //type safety sağlar yamnlışıkla float atamsassın

    // Motor yıpranmasındaki sabit çarpanlar.
    static constexpr float WEAR_EGT = 0.18f;
    static constexpr float WEAR_YAKIT = 0.12f;
    static constexpr float WEAR_YAG_BASINCI = -0.30f;
    static constexpr float WEAR_YAG_SICAKLIGI = 0.15f;
    static constexpr float WEAR_TITRESIM = 1.5f;

    // Yıpranmanın etkisini anlık güce göre ölçekler -Rölantide az, tam güçte çok etki eder-
    static float WearScale(float power) { return 0.3f + 0.7f * power; }

    // TF10000'in fiziksel tepki modeli. Güç ve ortam sıcaklığına göre anlık değerleri hesaplar.
    void Engine_Start(float power, float lm35Temp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;
        const float w = WearFactor() * WearScale(power);

        param_Devir1 = 35.0f + (power * 65.0f);
        param_Devir2 = 65.0f + (power * 35.0f);
        param_Basinc = 2.0f  + (power * 18.0f);
        //LM35-> ortam sıcaksa etikler     W-> motor yıpranam katsayısı   WEAR_...-> araştırılan katsayılar
        param_EGT    = (480.0f + (power * 470.0f) + (lm35Temp - 25.0f)) * (1.0f + w * WEAR_EGT);
        param_Yakit  = (250.0f + (power * 2850.0f)) * (1.0f + w * WEAR_YAKIT);
        param_YagBasinci   = (40.0f + (power * 30.0f)) * (1.0f + w * WEAR_YAG_BASINCI);
        param_YagSicakligi = (60.0f + (power * 60.0f)) * (1.0f + w * WEAR_YAG_SICAKLIGI);
        param_Titresim     = (1.5f  + (power * 4.5f)) * (1.0f + w * WEAR_TITRESIM);
    }


    EngineAlarmLevel EvaluateAlarm() const override {    // TF10000 için kritik eşiklere göre alarm seviyesini döner.
        EngineAlarmLevel level = EngineAlarmLevel::Normal;
        level = worstOf(level, bandHigh(param_Devir1, 95.0f, 105.0f));
        level = worstOf(level, bandHigh(param_Devir2, 95.0f, 105.0f));
        level = worstOf(level, bandRange(param_YagBasinci, 35.0f, 30.0f, 75.0f, 80.0f));
        level = worstOf(level, bandHigh(param_Titresim, 4.0f, 5.0f));
        level = worstOf(level, bandHigh(param_EGT, 775.0f, 850.0f));
        return level;
    }


    SpoolProfile GetSpoolProfile() const override {     // TF10000  için yükseliş ve düşüş atalet süreleri.
        return SpoolProfile{
            10.0, 90.0,  // devir1 (N1 Fan): Ağır kütle, geç
            5.0,  60.0,  // devir2 (N2 Core): Hafif kütle, hızlı
            5.0,  60.0,  // basinc
            8.0,  40.0,  // egt: Yakıt kesilince hızlı düşer,
            1.5,  0.5,   // yakit: anlıktır
            5.0,  60.0,  // yagBasinci
            60.0, 240.0, // yagSicakligi:  soğuması uzun sürer.
            10.0, 90.0   // titresim
        };
    }

    ParamCeilings GetParamCeilings() const override {    // Maksimum güçteki tavan değerleri (Yıpranmayı dahil et).

        const float w = WearFactor();
        return ParamCeilings{
            100.0, 100.0, 20.0,
            950.0 * (1.0 + w * WEAR_EGT),
            3100.0 * (1.0 + w * WEAR_YAKIT),
            70.0 * (1.0 + w * WEAR_YAG_BASINCI),
            120.0 * (1.0 + w * WEAR_YAG_SICAKLIGI),
            6.0 * (1.0 + w * WEAR_TITRESIM)
        };
    }

    WearDeviations GetWearDeviations(float power) const override {    // Anlık güce göre yıpranma sapmalarının yüzdelik hesaplaması.

        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        const double w = WearFactor() * WearScale(power);
        return WearDeviations{
            w * WEAR_EGT * 100.0,
            w * WEAR_YAKIT * 100.0,
            w * WEAR_YAG_BASINCI * 100.0,
            w * WEAR_YAG_SICAKLIGI * 100.0,
            w * WEAR_TITRESIM * 100.0
        };
    }
};
// PD170 Sınıfı İçine Eklenecek Yıpranma Sabitleri
// PD170 Sınıfı--
class PD170 : public Engine {
public:
    // PD170 için filo/yaş değişkeni
    explicit PD170(float ageYears = 0.0f) : Engine("PD170") { this->ageYears = ageYears; }

    // Turbodizel Yıpranma Çarpanları
    static constexpr float WEAR_YAKIT = 0.08f;
    static constexpr float WEAR_YAG_BASINCI = -0.20f;
    static constexpr float WEAR_YAG_SICAKLIGI = 0.12f;
    static constexpr float WEAR_TITRESIM = 0.80f;
    static constexpr float WEAR_EGT = 0.10f;
    static constexpr float WEAR_BASINC = 0.05f;

    static float WearScale(float power) { return 0.2f + 0.8f * power; }

    void Engine_Start(float power, float lm35Temp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;
        const float w = WearFactor() * WearScale(power);

        param_Devir1 = 1000.0f + (power * 1800.0f);
        param_Devir2 = 70.0f + (power * 35.0f);
        param_Basinc = (1.0f  + (power * 1.6f)) * (1.0f + w * WEAR_BASINC);
        param_EGT    = (250.0f + (power * 350.0f) + (lm35Temp - 25.0f)) * (1.0f + w * WEAR_EGT);
        param_Yakit  = (4.0f  + (power * 32.0f)) * (1.0f + w * WEAR_YAKIT);
        param_YagBasinci   = (2.5f  + (power * 3.0f)) * (1.0f + w * WEAR_YAG_BASINCI);
        param_YagSicakligi = (70.0f + (power * 45.0f)) * (1.0f + w * WEAR_YAG_SICAKLIGI);
        param_Titresim     = (1.0f  + (power * 2.0f)) * (1.0f + w * WEAR_TITRESIM);
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
            /*devir1Rise*/       3.0,  /*devir1Fall*/        5.0,
            /*devir2Rise*/       90.0, /*devir2Fall*/        200.0,
            /*basincRise*/       1.0,  /*basincFall*/        1.0,
            /*egtRise*/          5.0,  /*egtFall*/           15.0,
            /*yakitRise*/        1.0,  /*yakitFall*/         0.5,
            /*yagBasinciRise*/   3.0,  /*yagBasinciFall*/    5.0,
            /*yagSicakligiRise*/ 80.0, /*yagSicakligiFall*/  220.0,
            /*titresimRise*/     3.0,  /*titresimFall*/      5.0
        };
    }

    ParamCeilings GetParamCeilings() const override {
        const float w = WearFactor();
        return ParamCeilings{
            /*devir1*/ 2800.0, /*devir2*/ 105.0,
            /*basinc*/ 2.6 * (1.0 + w * WEAR_BASINC),
            /*egt*/ 600.0 * (1.0 + w * WEAR_EGT),
            /*yakit*/ 36.0 * (1.0 + w * WEAR_YAKIT),
            /*yagBasinci*/ 5.5 * (1.0 + w * WEAR_YAG_BASINCI),
            /*yagSicakligi*/ 115.0 * (1.0 + w * WEAR_YAG_SICAKLIGI),
            /*titresim*/ 3.0 * (1.0 + w * WEAR_TITRESIM)
        };
    }

    WearDeviations GetWearDeviations(float power) const override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        const double w = WearFactor() * WearScale(power);
        return WearDeviations{
            w * WEAR_EGT * 100.0,
            w * WEAR_YAKIT * 100.0,
            w * WEAR_YAG_BASINCI * 100.0,
            w * WEAR_YAG_SICAKLIGI * 100.0,
            w * WEAR_TITRESIM * 100.0
        };
    }
};

#endif // ENGINE_CORE_H