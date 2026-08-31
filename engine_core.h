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
    // param_Titresim: gerçek bir IPS/mils ölçüsü değil, motora özgü dahili göreli şiddet skoru.
    // Motorlar arası ölçek farklı (TF10000 tavanı ~6, PD170 tavanı ~3), doğrudan karşılaştırılamaz.
    bool alarmState;
    float ageYears = 0.0f; // Motorun yaşı (Yıpranma simülasyonu için)

    Engine(std::string name) { engineName = name; alarmState = false; }
    virtual ~Engine() {}

    // Alt sınıfların (TF10000, PD170) kendine has motor tepkilerini yazacağı saf sanal fonksiyon.
    // sensorTemp: STM32 karttan (Gömülü.cpp.txt) gelen ham sıcaklık - ortam değil,
    // EGT_SENSOR_GAIN ile motora özgü oranlanarak doğrudan EGT'ye yansıtılır.
    virtual void Engine_Start(float power, float sensorTemp) = 0;


    float WearFactor() const {      // Motorun yaşa göre eponantional yıpranma faktörü hesaplar 0-1 arası
        const float tau = 8.0f;
        const float t = ageYears < 0.0f ? 0.0f : ageYears; // Negatif yaş (savunma amaçlı) fiziksel olmayan sonuç üretmesin diye 0'a sabitlenir.
        return 1.0f - std::exp(-t / tau);
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

    // STM sensorunun 25°C referanstan sapmasi, EGT'ye 1:1 degil bu kazancla yansir -
    // 470 (bu motorun EGT araligi: 480 rolanti -> 950 tam guc) / 100 = 4.7. Yani
    // sensor 100 derece sapsa EGT'yi kendi tam araliginca kaydirir (bkz. PD170'deki
    // farkli kazancla kiyasla - "motora gore oranli" demek budur).
    static constexpr float EGT_SENSOR_GAIN = 4.7f;

    // Yıpranmanın etkisini anlık güce göre ölçekler -Rölantide az, tam güçte çok etki eder-
    static float WearScale(float power) { return 0.3f + 0.7f * power; }

    // TF10000'in fiziksel tepki modeli. Güç ve STM sensöründen gelen EGT girdisine göre anlık değerleri hesaplar.
    void Engine_Start(float power, float sensorTemp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;
        const float w = WearFactor() * WearScale(power);

        param_Devir1 = 35.0f + (power * 65.0f);
        param_Devir2 = 65.0f + (power * 35.0f);
        param_Basinc = 2.0f  + (power * 18.0f); // birimsiz basınç oranı (EPR benzeri), gerçek psi/bar karşılığı yok
        // sensorTemp -> STM karttan gelen ham sicaklik, EGT_SENSOR_GAIN ile oranlanarak dogrudan EGT'ye yansir (ortam degil)
        // W -> motor yıpranam katsayısı   WEAR_...-> araştırılan katsayılar
        param_EGT    = (480.0f + (power * 470.0f) + (sensorTemp - 25.0f) * EGT_SENSOR_GAIN) * (1.0f + w * WEAR_EGT);
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
        // Yağ aşırı ısınması gerçek uçuş güvenliği riski (yağ film kaybı, yatak hasarı) - EGT gibi anlık alarma bağlandı.
        level = worstOf(level, bandHigh(param_YagSicakligi, 100.0f, 110.0f));
        // Yakıt debisi bilinçli olarak alarm dışı: anlık yüksekliği tehlikeli değil, sapması ancak trend (ECTM) ile anlamlı - wearNotes zaten bunu izliyor.
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

    // TF10000'deki gibi ama bu motorun kendi EGT araligina gore: 350 (250 rolanti
    // -> 600 tam guc) / 100 = 3.5. Ayni sensor sapmasi, iki motorda ORANTILI ama
    // farkli buyuklukte bir EGT kaymasi yaratir - "motora gore oranli" bu yuzden.
    static constexpr float EGT_SENSOR_GAIN = 3.5f;

    static float WearScale(float power) { return 0.2f + 0.8f * power; } // TF10000'e göre rölantide biraz daha fazla etki bırakıyor.

    // PD170'in fiziksel tepki modeli - kalıp TF10000 ile aynı: [güce bağlı taban] * [1 + yıpranma*katsayı].
    void Engine_Start(float power, float sensorTemp) override {
        if (power < 0.0f) power = 0.0f;
        if (power > 1.0f) power = 1.0f;
        powerLevel = power;
        const float w = WearFactor() * WearScale(power);

        param_Devir1 = 1000.0f + (power * 1800.0f);
        param_Devir2 = 70.0f + (power * 35.0f); // DİKKAT: Bu motorda RPM değil, soğutma suyu sıcaklığıdır.
        param_Basinc = (1.0f  + (power * 1.6f)) * (1.0f + w * WEAR_BASINC); // bar cinsinden mutlak turbo basıncı - WEAR_BASINC sadece PD170'de var (turbo basıncı yıpranmadan etkilenir).
        // sensorTemp -> STM karttan gelen ham sicaklik, EGT_SENSOR_GAIN ile oranlanarak dogrudan EGT'ye yansir (ortam degil)
        param_EGT    = (250.0f + (power * 350.0f) + (sensorTemp - 25.0f) * EGT_SENSOR_GAIN) * (1.0f + w * WEAR_EGT);
        param_Yakit  = (4.0f  + (power * 32.0f)) * (1.0f + w * WEAR_YAKIT);
        param_YagBasinci   = (2.5f  + (power * 3.0f)) * (1.0f + w * WEAR_YAG_BASINCI);
        param_YagSicakligi = (70.0f + (power * 45.0f)) * (1.0f + w * WEAR_YAG_SICAKLIGI);
        param_Titresim     = (1.0f  + (power * 2.0f)) * (1.0f + w * WEAR_TITRESIM);
    }

    EngineAlarmLevel EvaluateAlarm() const override {    // PD170 için spesifik tehlike eşikleri.
        EngineAlarmLevel level = EngineAlarmLevel::Normal;
        level = worstOf(level, bandHigh(param_Devir1, 2500.0f, 2800.0f));
        level = worstOf(level, bandHigh(param_Devir2, 100.0f, 120.0f));  // Soğutma suyu harareti kontrolü
        level = worstOf(level, bandRange(param_YagBasinci, 2.0f, 1.5f, 5.8f, 6.0f));
        level = worstOf(level, bandHigh(param_Titresim, 2.2f, 2.7f));
        level = worstOf(level, bandHigh(param_EGT, 480.0f, 550.0f));
        // Overboost gerçek bir turbo-dizelde kritik arıza modu (aşırı silindir basıncı, contra hasarı) - bu yüzden alarma bağlandı.
        level = worstOf(level, bandHigh(param_Basinc, 2.3f, 2.5f));
        // Yakıt debisi TF10000'deki gibi bilinçli olarak alarm dışı: anlık değeri değil, trend'i (ECTM/wearNotes) anlamlı.
        return level;
    }

    SpoolProfile GetSpoolProfile() const override {    // PD170 için atalet süreleri (kompresyon freni var, devir hızlı düşer).
        return SpoolProfile{
            /*devir1Rise*/       3.0,  /*devir1Fall*/        5.0,   // Krank RPM: atalet düşük, hızlı tepki.
            /*devir2Rise*/       90.0, /*devir2Fall*/        200.0, // Su sıcaklığı: termal kütle, yavaş değişir.
            /*basincRise*/       1.0,  /*basincFall*/        1.0,   // Kelebek tepkisi anlık.
            /*egtRise*/          5.0,  /*egtFall*/           15.0,  // Egzoz gazı, hızlı düşer.
            /*yakitRise*/        1.0,  /*yakitFall*/         0.5,   // Enjeksiyon pompası, neredeyse anlık.
            /*yagBasinciRise*/   3.0,  /*yagBasinciFall*/    5.0,   // Direkt RPM'e bağlı, hızlı düşer.
            /*yagSicakligiRise*/ 80.0, /*yagSicakligiFall*/  220.0, // Büyük yağ hacmi, yavaş.
            /*titresimRise*/     3.0,  /*titresimFall*/      5.0    // Piston ateşleme titreşimi, RPM'e bağlı.
        };
    }

    ParamCeilings GetParamCeilings() const override {    // Maksimum güçteki tavan değerleri (yıpranma payı dahil).
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

#endif // ENGINE_CORE_H