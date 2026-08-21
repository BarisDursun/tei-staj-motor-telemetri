#ifndef ENGINEMODEL_H
#define ENGINEMODEL_H

#include <QObject>
#include <QString>
#include <QVariant>
#include "engine_core.h"

class QTimer;

class EngineModel : public QObject {
    Q_OBJECT

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
    // Anlik calisma guvenligi (alarmLevel) DEGIL - motorun yasina/asinmasina
    // dayali uzun vadeli bakim durumu (bkz. engine_core.h MaintenanceStatus).
    Q_PROPERTY(QString maintenanceStatusText READ maintenanceStatusText NOTIFY maintenanceStatusChanged)

public:
    // Engine::EvaluateAlarm()'daki EngineAlarmLevel ile ayni sirada (Qt tarafi
    // bagimsiz kalsin diye Engine sinifi Qt'ye bagli degil, burada Q_ENUM ile
    // QML'e aciyoruz).
    enum class AlarmLevel { Normal, Warning, Critical };
    Q_ENUM(AlarmLevel)

    explicit EngineModel(QObject *parent = nullptr);
    ~EngineModel();

    Q_INVOKABLE void selectEngine(const QString &engineName);
    // TF10000 filosundaki 10 sabit yastaki motoru {id, label, ageYears,
    // maintenanceStatusText} olarak dondurur - QML bir Repeater ile listeler.
    Q_INVOKABLE QVariantList fleetEngines() const;
    // Filodan belirli bir motoru (id ile) yukler - o motorun yasina gore
    // asinmis parametreleriyle simulasyona baslar.
    Q_INVOKABLE void selectFleetEngine(int fleetId);
    // Gaz kolu (throttle) hedefini ayarlar - motor calisirken anlik uygulanmaz,
    // her tikte m_actualPower bu hedefe dogru asamali yaklasir.
    Q_INVOKABLE void setPower(double powerPercent);
    // Motoru "kapali"dan "calisiyor"a, yavasca spool-up ile geciren komut.
    Q_INVOKABLE void startEngine();
    // Motoru calisir durumdan spool-down ile gercekten sifira indiren komut.
    Q_INVOKABLE void stopEngine();

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
    QString maintenanceStatusText() const { return m_maintenanceStatusText; }

signals:
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
    // Periyodik simulasyon adimi: spool-up/down rampasini ilerletir, gaz kolu
    // hedefine dogru guc rampasini uygular ve kucuk gurultu ekler.
    void simulationTick();

private:
    void refreshFromEngine(bool withJitter);

    void setDevir1(double v);
    void setDevir2(double v);
    void setBasinc(double v);
    void setEgt(double v);
    void setYakit(double v);
    void setYagBasinci(double v);
    void setYagSicakligi(double v);
    void setTitresim(double v);
    void setAlarmLevel(AlarmLevel v);
    // Yeni bir Engine (selectEngine ya da selectFleetEngine ile) yuklendiginde
    // ortak sifirlama/ilk-frame kodu - kod tekrarini onlemek icin ayrildi.
    void resetSimulationStateFor(Engine *newEngine);

    Engine *currentEngine = nullptr;
    QTimer *m_simTimer = nullptr;

    bool m_running = false;       // start/stop ile degisir, spool hedefini belirler
    double m_targetPower = 0.0;   // slider'dan gelen anlik gaz kolu komutu (%)
    double m_actualPower = 0.0;   // hedefe dogru asamali yaklasan gercek guc (%)

    // Her parametrenin kendi "ne kadar spool oldu" carpani (0=kapali, 1=tam
    // calisir) - hizi Engine::GetSpoolProfile()'daki rise/fall sürelerinden
    // gelir, motor tipine gore degisir (bkz. engine_core.h).
    double m_factorDevir1 = 0.0;
    double m_factorDevir2 = 0.0;
    double m_factorBasinc = 0.0;
    double m_factorEgt = 0.0;
    double m_factorYakit = 0.0;
    double m_factorYagBasinci = 0.0;
    double m_factorYagSicakligi = 0.0;
    double m_factorTitresim = 0.0;

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
};

#endif // ENGINEMODEL_H
