#include <QtTest>
#include "enginemodel.h"

// Kategoriler: A) baslangic durumu  B) esik/sinir deger matrisi
// C) sinyal disiplini (NOTIFY)      D) resetAlarm davranisi
// E) uc durum / dayaniklilik        F) vibration
class TestEngineModel : public QObject {
    Q_OBJECT

private slots:

    // ---------- A. Baslangic durumu ----------

    void freshModelStartsBelowThreshold() {
        EngineModel model;
        QVERIFY2(!model.alarmState(),
                 "Varsayilan sicaklik (20C) esigin (90C) altinda, alarm baslangicta kapali olmali");
    }

    // ---------- B. Esik / sinir deger matrisi ----------

    void alarmThreshold_data() {
        QTest::addColumn<double>("temperature");
        QTest::addColumn<bool>("expectedAlarm");

        QTest::newRow("negatif sicaklik")      << -40.0   << false;
        QTest::newRow("donma noktasi")         << 0.0     << false;
        QTest::newRow("soguk")                 << 20.0    << false;
        QTest::newRow("ilik")                  << 70.0    << false;
        QTest::newRow("esigin hemen alti")     << 89.9    << false;
        QTest::newRow("esigin cok hemen alti") << 89.999  << false;
        QTest::newRow("tam esikte")            << 90.0    << false;  // ">" kullaniliyor, ">=" degil
        QTest::newRow("esigin cok hemen ustu") << 90.001  << true;
        QTest::newRow("esigin hemen ustu")     << 90.1    << true;
        QTest::newRow("sicak")                 << 105.0   << true;
        QTest::newRow("cok sicak")             << 120.0   << true;
        QTest::newRow("asiri deger")           << 1000.0  << true;
    }

    void alarmThreshold() {
        QFETCH(double, temperature);
        QFETCH(bool, expectedAlarm);

        EngineModel model;
        model.setTemperatureForTest(temperature);
        QCOMPARE(model.alarmState(), expectedAlarm);
    }

    // ---------- C. Sinyal disiplini (NOTIFY) ----------

    void temperatureChangedFiresOnSet() {
        EngineModel model;
        QSignalSpy spy(&model, &EngineModel::temperatureChanged);
        model.setTemperatureForTest(30.0);
        QCOMPARE(spy.count(), 1);
    }

    void temperatureChangedFiresEvenIfValueUnchanged() {
        // Acik soru: mevcut implementasyon deger degismese bile kosulsuz
        // emit ediyor. Bu test mevcut davranisi belgeliyor - QML tarafinda
        // gereksiz binding yeniden hesaplamasina yol acabilir, Kisi 1 ile
        // konusulmali (genelde "if (value == m_x) return;" ile onlenir).
        EngineModel model;
        model.setTemperatureForTest(50.0);
        QSignalSpy spy(&model, &EngineModel::temperatureChanged);
        model.setTemperatureForTest(50.0);
        QCOMPARE(spy.count(), 1);
    }

    void alarmStateChangedFiresOnlyOnTransition() {
        EngineModel model;
        QSignalSpy spy(&model, &EngineModel::alarmStateChanged);

        model.setTemperatureForTest(95.0);   // false -> true : 1 emit
        model.setTemperatureForTest(100.0);  // true  -> true : emit YOK
        model.setTemperatureForTest(110.0);  // true  -> true : emit YOK

        QCOMPARE(spy.count(), 1);
        QVERIFY(model.alarmState());
    }

    void alarmStateChangedFiresOnReturnToNormal() {
        EngineModel model;
        model.setTemperatureForTest(95.0);
        QSignalSpy spy(&model, &EngineModel::alarmStateChanged);
        model.setTemperatureForTest(50.0);   // true -> false
        QCOMPARE(spy.count(), 1);
        QVERIFY(!model.alarmState());
    }

    // ---------- D. resetAlarm davranisi ----------

    void resetAlarmClearsActiveAlarm() {
        EngineModel model;
        model.setTemperatureForTest(95.0);
        QVERIFY(model.alarmState());
        model.resetAlarm();
        QVERIFY(!model.alarmState());
    }

    void resetAlarmEmitsSignalWhenClearingActiveAlarm() {
        EngineModel model;
        model.setTemperatureForTest(95.0);
        QSignalSpy spy(&model, &EngineModel::alarmStateChanged);
        model.resetAlarm();
        QCOMPARE(spy.count(), 1);
    }

    void resetAlarmWhileStillHot_reTriggersOnNextReading() {
        // Tasarim sorusu: sicaklik hala esigin ustundeyken resetAlarm()
        // cagrilirsa, mevcut implementasyon bir sonraki okumada alarmi
        // hemen tekrar tetikliyor (m_alarmState false'a duser, sonraki
        // adimda newAlarm=true tekrar farkli bulunur). Bu istenen davranis
        // mi, yoksa sicaklik gercekten esigin altina dusene kadar alarmin
        // sessiz kalmasi mi gerekiyor - Kisi 1 ile netlestirilmeli.
        EngineModel model;
        model.setTemperatureForTest(95.0);
        model.resetAlarm();
        QVERIFY(!model.alarmState());

        model.setTemperatureForTest(95.0);  // ayni yuksek deger, "bir sonraki okuma"
        QVERIFY2(model.alarmState(),
                 "Mevcut implementasyonda reset sonrasi sicaklik hala esigin "
                 "ustundeyse alarm bir sonraki okumada tekrar tetikleniyor");
    }

    // ---------- E. Uc durum / dayaniklilik ----------

    void extremeNegativeTemperatureDoesNotCrash() {
        EngineModel model;
        model.setTemperatureForTest(-1.0e9);
        QVERIFY(!model.alarmState());
        QCOMPARE(model.temperature(), -1.0e9);
    }

    void extremePositiveTemperatureDoesNotCrash() {
        EngineModel model;
        model.setTemperatureForTest(1.0e9);
        QVERIFY(model.alarmState());
        QCOMPARE(model.temperature(), 1.0e9);
    }

    // ---------- F. Vibration ----------

    void freshModelHasZeroVibration() {
        EngineModel model;
        QCOMPARE(model.vibration(), 0.0);
    }

    void vibrationChangedFiresOnSet() {
        EngineModel model;
        QSignalSpy spy(&model, &EngineModel::vibrationChanged);
        model.setVibrationForTest(2.5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.vibration(), 2.5);
    }
};

QTEST_MAIN(TestEngineModel)
#include "test_enginemodel.moc"
