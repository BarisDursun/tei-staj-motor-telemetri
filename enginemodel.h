#ifndef ENGINEMODEL_H                    //Bridge / ViewMode   header
#define ENGINEMODEL_H          //birden fazla kez #include ederse, çakışma (redefinition) hatası vermesini engeller.

#include <QObject>
#include <QString>
#include <QVariant>              //QML--C++ arasındaki data transformation bridge
#include "engine_core.h"

class QTimer;

// QObject'ten miras alıyoruz ki Qt'nin Meta-Object yeteneklerini (Signal/Slot, Property) kullanalım.
class EngineModel : public QObject {
    Q_OBJECT // Qt'nin MOC (Meta-Object Compiler)de sistemini devreye sokan kritik makro.

    // Q_PROPERTY'ler UI (QML) & C++ arasındaki  bridge
    // QML de engineModel.p_EGT okunduğunda C++'daki egt() fonksiyonu çağrılır.
    // Değer değiştiğinde egtChanged sinyali fırlatılır QML'in arayüzü güncellemesi sağlanır.
    Q_PROPERTY(double p_Devir1       READ devir1       NOTIFY devir1Changed)
    Q_PROPERTY(double p_Devir2       READ devir2       NOTIFY devir2Changed)
    Q_PROPERTY(double p_Basinc       READ basinc       NOTIFY basincChanged)
    Q_PROPERTY(double p_EGT          READ egt          NOTIFY egtChanged)
    Q_PROPERTY(double p_Yakit        READ yakit        NOTIFY yakitChanged)
    Q_PROPERTY(double p_YagBasinci   READ yagBasinci   NOTIFY yagBasinciChanged)
    Q_PROPERTY(double p_YagSicakligi READ yagSicakligi NOTIFY yagSicakligiChanged)
    Q_PROPERTY(double p_Titresim     READ titresim     NOTIFY titresimChanged)

    Q_PROPERTY(AlarmLevel alarmLevel     READ alarmLevel     NOTIFY alarmLevelChanged)
    Q_PROPERTY(QString    alarmLevelText READ alarmLevelText NOTIFY alarmLevelChanged)

    //  yaşına/aşınmaya dayalı uzun vadeli bakım durumu.
    Q_PROPERTY(QString maintenanceStatusText READ maintenanceStatusText NOTIFY maintenanceStatusChanged)

    // QVariantList: QML de JavaScript dizisi (array)  -Aşınma notlarını liste halinde UI'a aktarır.
    Q_PROPERTY(QVariantList wearNotes READ wearNotes NOTIFY maintenanceStatusChanged)



public:
    enum class AlarmLevel { Normal, Warning, Critical };      // QML tarafında  enum'u kullanabilmek için
    Q_ENUM(AlarmLevel)

    explicit EngineModel(QObject *parent = nullptr);
    ~EngineModel();

    // Q_INVOKABLE: QML tarafından c++ a method çağırma
    Q_INVOKABLE void selectEngine(const QString &engineName);   //Motor Seçimi

    Q_INVOKABLE QVariantList fleetEngines() const;      // Filodaki motorların listesini besler
    Q_INVOKABLE void selectFleetEngine(int fleetId);

    Q_INVOKABLE void setPower(double powerPercent);    // Gaz kolu
    Q_INVOKABLE void startEngine();
    Q_INVOKABLE void stopEngine();

    // Q_PROPERTY'lerin READ fonksiyonları. Değişkenleri güvenli bir şekilde dışarı açar (Encapsulation).
    double devir1() const       { return m_devir1; }
    double devir2() const       { return m_devir2; }
    double basinc() const       { return m_basinc; }
    double egt() const          { return m_egt; }
    double yakit() const        { return m_yakit; }
    double yagBasinci() const   { return m_yagBasinci; }
    double yagSicakligi() const { return m_yagSicakligi; }
    double titresim() const     { return m_titresim; }
    AlarmLevel alarmLevel() const { return m_alarmLevel; }
    QString alarmLevelText() const;

    //  Motor test edilmeden teşhisi gösterme
    QString maintenanceStatusText() const { return m_tested ? m_maintenanceStatusText : QStringLiteral("HENÜZ TEST EDİLMEDİ"); }  //runtime de  zaandan kazanc
    QVariantList wearNotes() const { return m_tested ? m_wearNotes : QVariantList(); }

signals: // status changingi QML'e (veya diğer C++ sınıflarına) haber veren sinyaller.
    void devir1Changed();
    void devir2Changed();
    void basincChanged();
    void egtChanged();
    void yakitChanged();
    void yagBasinciChanged();
    void yagSicakligiChanged();
    void titresimChanged();
    void alarmLevelChanged();
    void maintenanceStatusChanged();

private slots:
    // QTimer tarafından periyodik simulation loop
    void simulationTick();

private:
    void refreshFromEngine(bool withJitter);   //anlık değişim için

    // Q_PROPERTY değerlerini güncelleyen ve sonrasında NOTIFY sinyallerini tetikleyen iç fonksiyonlar (Setter).
    void setDevir1(double v);
    void setDevir2(double v);
    void setBasinc(double v);
    void setEgt(double v);
    void setYakit(double v);
    void setYagBasinci(double v);
    void setYagSicakligi(double v);
    void setTitresim(double v);
    void setAlarmLevel(AlarmLevel v);

    void resetSimulationStateFor(Engine *newEngine);

    Engine *currentEngine = nullptr;    // Farklı motorlar runtime da bağlanır
    QTimer *m_simTimer = nullptr; // Simülasyonun kalp atışını sağlayan zamanlayıcı.

    bool m_running = false;       // Motor çalışıyor mu?
    double m_targetPower = 0.0;   //  UI'dan gelen gaz seviyesi.
    double m_actualPower = 0.0;   // Motorun anlık fiziksel ı güç seviyesi.

    // Atalet Çarpanları hedefe ne kadar yaklaşır 0-1
    double m_factorDevir1 = 0.0;
    double m_factorDevir2 = 0.0;
    double m_factorBasinc = 0.0;
    double m_factorEgt = 0.0;
    double m_factorYakit = 0.0;
    double m_factorYagBasinci = 0.0;
    double m_factorYagSicakligi = 0.0;
    double m_factorTitresim = 0.0;

    // Sınıf içi gizli değişkenler (Backing fields).
    double m_devir1 = 0.0;
    double m_devir2 = 0.0;
    double m_basinc = 1.0;
    double m_egt = 20.0;
    double m_yakit = 0.0;
    double m_yagBasinci = 0.0;
    double m_yagSicakligi = 20.0;
    double m_titresim = 0.0;

    AlarmLevel m_alarmLevel = AlarmLevel::Normal;
    QString m_maintenanceStatusText = QStringLiteral("SAĞLIKLI");
    QVariantList m_wearNotes;

    bool m_tested = false;      // check flag Yıpranma verisinin UI'da görünmesini kontrol eder.

};

#endif // ENGINEMODEL_H