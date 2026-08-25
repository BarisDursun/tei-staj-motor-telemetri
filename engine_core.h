#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H

#include <string>
#include <cmath>

// Motorun anlık tehlike durumunu belirten enum. Qt'den bağımsızdır.
enum class EngineAlarmLevel { Normal, Warning, Critical };

// Motorun uzun vadeli yıpranma (kondisyon) durumu. Anlık tehlike (alarm) ile karıştırılmamalıdır.
enum class MaintenanceStatus { Healthy, Watch, MaintenanceRequired };

// Motor parametrelerinin hedefe ulaşma (rise) ve düşme (fall) sürelerini tutar.
// Termal atalet ve mekanik kütle farklarından dolayı yükseliş ve düşüş süreleri farklıdır.
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

// Motorun %100 güçte ulaşabileceği maksimum fiziksel tavan değerleri.
// Gürültü (jitter) eklendiğinde değerlerin saçmalamasını engeller.
struct ParamCeilings {
    double devir1, devir2, basinc, egt, yakit, yagBasinci, yagSicakligi, titresim;
};

// Yıpranmaya bağlı olarak parametrelerin "sıfır motor" referansından yüzde kaç saptığını tutar.
struct WearDeviations {
    double egtPct, yakitPct, yagBasinciPct, yagSicakligiPct, titresimPct;
};

// Polimorfizm için temel (Base) Motor Sınıfı. Tüm motorlar bu sınıftan türetilir.
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

    // Motorun yaşına göre logaritmik bir yıpranma faktörü hesaplar (0.0 ile 1.0 arası).
    float WearFactor() const {
        const float tau = 8.0f;
        return 1.0f - std::exp(-ageYears / tau);
    }

    // Yıpranma faktörüne göre otomatik bakım durumunu belirler. enumda
    MaintenanceStatus GetMaintenanceStatus() const {
        const float w = WearFactor();
        if (w >= 0.70f) return MaintenanceStatus::MaintenanceRequired;
        if (w >= 0.45f) return MaintenanceStatus::Watch;
        return MaintenanceStatus::Healthy;
    }

    // Polimorfik fonksiyonlar: Her motor kendi alarm eşiğini, spool profilini ve tavan değerlerini kendi belirler.
    virtual EngineAlarmLevel EvaluateAlarm() const = 0;
    virtual SpoolProfile GetSpoolProfile() const = 0;
    virtual ParamCeilings GetParamCeilings() const = 0;
    virtual WearDeviations GetWearDeviations(float power) const = 0;

protected:
    // İki alarm seviyesinden daha kötü olanı seçen yardımcı fonksiyon.
    static EngineAlarmLevel worstOf(EngineAlarmLevel a, EngineAlarmLevel b) {
        return static_cast<int>(b) > static_cast<int>(a) ? b : a;
    }
    // Değer arttıkça tehlikeli olan parametreler (örn: Titreşim, EGT) için alarm kontrolü.
    static EngineAlarmLevel bandHigh(float value, float warnAt, float critAt) {
        if (value >= critAt) return EngineAlarmLevel::Critical;
        if (value >= warnAt) return EngineAlarmLevel::Warning;
        return EngineAlarmLevel::Normal;
    }
    // Hem düşük hem yüksek değerin tehlikeli olduğu parametreler (örn: Yağ Basıncı) için alarm kontrolü.
    static EngineAlarmLevel bandRange(float value, float warnLow, float critLow, float warnHigh, float critHigh) {
        if (value <= critLow || value >= critHigh) return EngineAlarmLevel::Critical;
        if (value <= warnLow || value >= warnHigh) return EngineAlarmLevel::Warning;
        return EngineAlarmLevel::Normal;
    }
};

// Turbofan Motor Sınıfı (Engine'den miras alır)
class TF10000 : public Engine {
public:
    explicit TF10000(float ageYears = 0.0f) : Engine("TF10000") { this->ageYears = ageYears; }

    // Motor yıprandıkça parametrelerin nasıl etkileneceğini belirleyen sabit çarpanlar.
    static constexpr float WEAR_EGT = 0.18f;
    static constexpr float WEAR_YAKIT = 0.12f;
    static constexpr float WEAR_YAG_BASINCI = -0.30f;
    static constexpr float WEAR_YAG_SICAKLIGI = 0.15f;
    static constexpr float WEAR_TITRESIM = 1.5f;

    // Yıpranmanın etkisini anlık güce göre ölçekler (Rölantide az, tam güçte çok etki eder).
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
        param_EGT    = (480.0f + (power * 470.0f) + (lm35Temp - 25.0f)) * (1.0f + w * WEAR_EGT);
        param_Yakit  = (250.0f + (power * 2850.0f)) * (1.0f + w * WEAR_YAKIT);
        param_YagBasinci   = (40.0f + (power * 30.0f)) * (1.0f + w * WEAR_YAG_BASINCI);
        param_YagSicakligi = (60.0f + (power * 60.0f)) * (1.0f + w * WEAR_YAG_SICAKLIGI);
        param_Titresim     = (1.5f  + (power * 4.5f)) * (1.0f + w * WEAR_TITRESIM);
    }

    // TF10000 için kritik eşiklere göre alarm seviyesini döner.
    EngineAlarmLevel EvaluateAlarm() const override {
        EngineAlarmLevel level = EngineAlarmLevel::Normal;
        level = worstOf(level, bandHigh(param_Devir1, 95.0f, 105.0f));
        level = worstOf(level, bandHigh(param_Devir2, 95.0f, 105.0f));
        level = worstOf(level, bandRange(param_YagBasinci, 35.0f, 30.0f, 75.0f, 80.0f));
        level = worstOf(level, bandHigh(param_Titresim, 4.0f, 5.0f));
        level = worstOf(level, bandHigh(param_EGT, 775.0f, 850.0f));
        return level;
    }

    // TF10000 (Turbofan) için yükseliş ve düşüş atalet süreleri.
    SpoolProfile GetSpoolProfile() const override {
        return SpoolProfile{
            10.0, 90.0,  // devir1 (N1 Fan): Ağır kütle, geç durur.
            5.0,  60.0,  // devir2 (N2 Core): Hafif kütle, hızlı tepki.
            5.0,  60.0,  // basinc
            8.0,  40.0,  // egt: Yakıt kesilince hızlı düşer, termal sönümleme biraz uzatır.
            1.5,  0.5,   // yakit: Valf tepkisi, anlıktır.
            5.0,  60.0,  // yagBasinci
            60.0, 240.0, // yagSicakligi: Sıvı kütlesi, soğuması uzun sürer.
            10.0, 90.0   // titresim
        };
    }

    // Maksimum güçteki tavan değerleri (Yıpranma payları dahil edilerek hesaplanır).
    ParamCeilings GetParamCeilings() const override {
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

    // Mevcut güce göre yıpranma sapmalarının yüzdelik hesaplaması.
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

// Pistonlu/Dizel Havacılık Motoru Sınıfı (Engine'den miras alır)
class PD170 : public Engine {
public:
    PD170() : Engine("PD170") {}

    // PD170'in fiziksel tepki modeli. Parametre aralıkları turbofandan tamamen farklıdır.
    void Engine_Start(float power, float lm35Temp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;

        param_Devir1 = 1000.0f + (power * 1800.0f);
        param_Devir2 = 70.0f + (power * 35.0f); // DİKKAT: Bu motorda RPM değil, soğutma suyu sıcaklığıdır.
        param_Basinc = 1.0f  + (power * 1.6f);
        param_EGT    = 250.0f + (power * 350.0f) + (lm35Temp - 25.0f);
        param_Yakit  = 4.0f  + (power * 32.0f);
        param_YagBasinci   = 2.5f  + (power * 3.0f);
        param_YagSicakligi = 70.0f + (power * 45.0f);
        param_Titresim     = 1.0f  + (power * 2.0f);
    }

    // PD170 için spesifik tehlike eşikleri.
    EngineAlarmLevel EvaluateAlarm() const override {
        EngineAlarmLevel level = EngineAlarmLevel::Normal;
        level = worstOf(level, bandHigh(param_Devir1, 2500.0f, 2800.0f));
        level = worstOf(level, bandHigh(param_Devir2, 100.0f, 120.0f)); // Soğutma suyu harareti kontrolü
        level = worstOf(level, bandRange(param_YagBasinci, 2.0f, 1.5f, 5.8f, 6.0f));
        level = worstOf(level, bandHigh(param_Titresim, 2.2f, 2.7f));
        level = worstOf(level, bandHigh(param_EGT, 480.0f, 550.0f));
        return level;
    }

    // PD170 (Pistonlu) için atalet süreleri. Kompresyon frenlemesi sayesinde devirler daha hızlı düşer.
    SpoolProfile GetSpoolProfile() const override {
        return SpoolProfile{
            3.0,  5.0,   // devir1 (Krank RPM): Atalet düşük, kompresyon freni var, hızlı düşer.
            90.0, 200.0, // devir2 (Su Sıcaklığı): Termal kütle, çok yavaş değişir.
            1.0,  1.0,   // basinc: Kelebek tepkisi anlık.
            5.0,  15.0,  // egt: Egzoz gazı, hızlı söner.
            1.0,  0.5,   // yakit
            3.0,  5.0,   // yagBasinci: Direkt RPM'e bağlı, hızlı düşer.
            80.0, 220.0, // yagSicakligi
            3.0,  5.0    // titresim
        };
    }

    ParamCeilings GetParamCeilings() const override {
        return ParamCeilings{
            2800.0, 105.0, 2.6, 600.0,
            36.0, 5.5, 115.0, 3.0
        };
    }

    // PD170 için henüz yaş/yıpranma modeli eklenmediği için sapmalar 0 döner.
    WearDeviations GetWearDeviations(float /*power*/) const override {
        return WearDeviations{0.0, 0.0, 0.0, 0.0, 0.0};
    }
};

#endif // ENGINE_CORE_H