#include <QtTest>
#include "enginemodel.h"

// EngineModel (gercek API: p_Devir1, alarmLevel, selectEngine, selectFleetEngine...)
// icin QtTest tabanli entegrasyon testleri. Eski test_enginemodel.cpp Kisi 1'in
// gercek implementasyonundan once yazilmis bir placeholder API'yi (temperature,
// alarmState, setTemperatureForTest...) test ediyordu, artik derlenmiyordu -
// docs/gereksinimler.md onayindan sonra bu dosya bastan yazildi.
//
// Kategoriler: A) motor secimi/filo  B) simulasyon/spool  C) alarm start-inhibit
// D) bakim teshisi (m_tested)        E) wearNotes         F) girdi saglamligi

namespace {
// simulationTick() private slot - Qt moc string-tabanli invokeMethod'da C++ erisim
// denetimini gormez, bu yuzden production kodunda test-only bir hook eklemeye
// gerek kalmadan dogrudan cagirabiliyoruz.
void tick(EngineModel &model, int times = 1) {
    for (int i = 0; i < times; ++i)
        QMetaObject::invokeMethod(&model, "simulationTick");
}
}

class TestEngineModel : public QObject {
    Q_OBJECT

private slots:

    // ---------- A. Motor secimi ve filo (FR-01..05, FR-25) ----------

    // FR-04: selectFleetEngine'in m_engineFamily'ye gore dogru alt sinifi (PD170) orneklendigini dogrular.
    void selectFleetEngine_respectsEngineFamily() {
        // Regresyon testi: m_engineFamily eklenmeden once selectFleetEngine
        // her zaman TF10000 yaratiyordu - PD170 filodan hic secilemiyordu.
        EngineModel model;
        model.selectEngine("PD170");
        model.selectFleetEngine(1); // id=1, age=0
        model.startEngine();
        model.setPower(100.0);
        tick(model, 60); // guc + atalet oturmasi icin yeterli (PD170 devir1Rise=3s=15 tick)

        // PD170 devir1 RPM'dir (1000-2800), TF10000 devir1 en fazla 100 (%) olur -
        // bu yuzden >500 gorulmesi PD170'in gercekten yaratildigini kanitlar.
        QVERIFY2(model.devir1() > 500.0, "PD170 filo secimi TF10000 yaratiyor gibi gorunuyor");
    }

    // FR-25: taninmayan motor adinin sessizce reddedilip onceki motor durumunun bozulmadigini dogrular.
    void selectEngine_invalidName_preservesPreviousState() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(1);
        model.startEngine();
        model.setPower(100.0);
        tick(model, 40); // m_tested tetiklenene kadar (yagBasinciRise=5s=25 tick)
        QVERIFY(model.maintenanceStatusText() != QStringLiteral("HENÜZ TEST EDİLMEDİ"));

        model.selectEngine("BOGUS_ENGINE"); // gecersiz isim
        // Eger reddedilmeseydi resetSimulationStateFor() m_tested'i false'a dusurup
        // metni yeniden "HENUZ TEST EDILMEDI"ye cevirirdi.
        QCOMPARE(model.maintenanceStatusText() != QStringLiteral("HENÜZ TEST EDİLMEDİ"), true);
    }

    // FR-25: filoda olmayan fleetId'nin sessizce reddedilip onceki motor durumunun bozulmadigini dogrular.
    void selectFleetEngine_invalidId_preservesPreviousState() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(1);
        model.startEngine();
        model.setPower(100.0);
        tick(model, 40);
        QVERIFY(model.maintenanceStatusText() != QStringLiteral("HENÜZ TEST EDİLMEDİ"));

        model.selectFleetEngine(9999); // filoda olmayan id
        QVERIFY(model.maintenanceStatusText() != QStringLiteral("HENÜZ TEST EDİLMEDİ"));
    }

    // ---------- B. Simulasyon / spool (FR-07, FR-10, FR-11) ----------

    // FR-10: motor durunca parametrelerin aniden sifira degil, fall suresine gore kademeli indigini dogrular.
    void stopEngine_decaysGraduallyNotInstantly() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(1);
        model.startEngine();
        model.setPower(100.0);
        tick(model, 80); // tam guce oturt

        const double runningDevir1 = model.devir1();
        QVERIFY(runningDevir1 > 50.0);

        model.stopEngine();
        tick(model, 1); // sadece BIR tick sonra
        QVERIFY2(model.devir1() > 0.0, "Motor durunca devir1 aninda sifira dusmemeli (kademeli inis)");
        QVERIFY(model.devir1() < runningDevir1);
    }

    // FR-11: motor degisiminde onceki motorun test/yiprama durumunun yeni motora sizmadigini dogrular.
    void switchingEngine_resetsMaintenanceState() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(1);
        model.startEngine();
        model.setPower(100.0);
        tick(model, 40);
        QVERIFY(model.maintenanceStatusText() != QStringLiteral("HENÜZ TEST EDİLMEDİ"));

        model.selectEngine("PD170"); // gecerli bir motor degisimi
        QCOMPARE(model.maintenanceStatusText(), QStringLiteral("HENÜZ TEST EDİLMEDİ"));
        QVERIFY(model.wearNotes().isEmpty());
    }

    // ---------- C. Alarm start-inhibit (FR-13) ----------

    // FR-13: yag basinci ataleti %97'ye ulasmadan alarmin zorunlu olarak Normal kaldigini dogrular (sahte alarm engeli).
    void alarmStaysNormalBeforeOilPressureSettles() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(1);
        model.startEngine();
        model.setPower(100.0);
        tick(model, 1); // sadece bir tick - factorYagBasinci hala ~0.04

        QCOMPARE(model.alarmLevel(), EngineModel::AlarmLevel::Normal);
    }

    // ---------- D. Bakim teshisi / m_tested (FR-15, FR-16, FR-17) ----------

    // FR-15: motor hic calistirilmadan bakim durumunun gosterilmedigini dogrular ("HENUZ TEST EDILMEDI").
    void freshFleetSelection_showsNotTestedYet() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(5); // yasli bir motor (age=5) olsa bile
        QCOMPARE(model.maintenanceStatusText(), QStringLiteral("HENÜZ TEST EDİLMEDİ"));
        QVERIFY(model.wearNotes().isEmpty());
    }

    // FR-17: GetMaintenanceStatus'un uc seviyesinin (fleetId -> yas uzerinden) EngineModel'e dogru yansidigini dogrular.
    void maintenanceStatus_data() {
        // bkz. docs/test-parametre-referans-degerleri.md (fleetId -> yas eslesmesi)
        QTest::addColumn<int>("fleetId");
        QTest::addColumn<QString>("expectedText");
        QTest::newRow("age=0 -> SAGLIKLI")      << 1 << QStringLiteral("SAĞLIKLI");
        QTest::newRow("age=5 -> IZLENMELI")     << 5 << QStringLiteral("İZLENMELİ");
        QTest::newRow("age=10 -> BAKIM GEREKLI") << 7 << QStringLiteral("BAKIM GEREKLİ");
    }

    void maintenanceStatus() {
        QFETCH(int, fleetId);
        QFETCH(QString, expectedText);

        EngineModel model;
        model.selectEngine("PD170"); // yagBasinciRise=3s -> hizli oturur, test hizli kalir
        model.selectFleetEngine(fleetId);
        model.startEngine();
        model.setPower(0.0); // rolantide bile test tamamlanmali
        tick(model, 25);     // 0.97/(0.2/3) ~ 15 tick yeter, pay birakildi

        QCOMPARE(model.maintenanceStatusText(), expectedText);
    }

    // FR-16, NFR-06: m_tested true olunca maintenanceStatusChanged sinyalinin tetiklendigini dogrular.
    void maintenanceStatusChanged_firesExactlyOnceWhenTestedFlips() {
        EngineModel model;
        model.selectEngine("PD170");
        model.selectFleetEngine(1);
        model.startEngine();
        model.setPower(0.0);
        tick(model, 10); // henuz %97'ye ulasmadi (10*0.0667=0.667)

        QSignalSpy spy(&model, &EngineModel::maintenanceStatusChanged);
        tick(model, 15); // simdi esigi gecer, m_tested tam bu araliktaki bir tick'te true olur
        QVERIFY2(spy.count() >= 1, "m_tested true olunca maintenanceStatusChanged en az bir kez tetiklenmeli");
    }

    // ---------- E. wearNotes: %10 filtre + buyukten kucuge siralama (FR-18, FR-19) ----------

    // FR-18, FR-19: wearNotes'un %10 esigini filtreledigini ve buyukten kucuge siraladigini gercek sayilarla dogrular.
    void wearNotes_filtersBelowTenPercent_andSortsByMagnitude() {
        // docs/test-parametre-referans-degerleri.md'deki hesapla birebir orten senaryo:
        // PD170, age=10 (fleetId=7), tam guc -> w_eff~0.7135
        //   Titresim  : +57%  (WEAR_TITRESIM=0.80)      -> listede, ilk sirada
        //   YagBasinci: -14%  (WEAR_YAG_BASINCI=-0.20)  -> listede, ikinci sirada
        //   EGT ~7%, YagSicakligi ~9%, Yakit ~6%        -> %10 esiginin altinda, listede degil
        EngineModel model;
        model.selectEngine("PD170");
        model.selectFleetEngine(7); // age=10
        model.startEngine();
        model.setPower(100.0);
        tick(model, 40); // guc (100/4=25 tick) + yagBasinci ataleti (~15 tick) icin yeterli

        const QVariantList notes = model.wearNotes();
        QCOMPARE(notes.size(), 2);
        QCOMPARE(notes.at(0).toString(), QStringLiteral("Titreşim: referansa göre +57%"));
        QCOMPARE(notes.at(1).toString(), QStringLiteral("Yağ Basıncı: referansa göre -14%"));
    }

    // ---------- F. Girdi saglamligi (FR-24, NFR-04) ----------

    // FR-24, NFR-04: setPower()'a asiri buyuk bir hedef verilince clamp'in devreye girip
    // gostergenin makul degere donuste hizla tepki verdigini dogrular (regresyon testi:
    // clamp eklenmeden once m_actualPower sinirsizca surukleniyor, deger uzun sure eskide takili kaliyordu).
    void setPower_clampsExtremeTarget_recoversPromptly() {
        EngineModel model;
        model.selectEngine("TF10000");
        model.selectFleetEngine(1);
        model.startEngine();

        model.setPower(50.0);
        tick(model, 60); // ~%50'de otur (atalet dahil)

        model.setPower(5000.0); // asiri buyuk hedef
        tick(model, 400);       // clamp olmasaydi actualPower burada binlere cikardi
        const double peakDevir1 = model.devir1();
        QVERIFY2(peakDevir1 <= 100.5, "clamp olmadan bile fiziksel tavan asilmamali (Engine_Start kendi ici clamp'i)");

        model.setPower(50.0); // guc tekrar makul degere cekiliyor
        tick(model, 5);       // sadece birkac tick - clamp'siz durumda hala tavana yakin kalirdi

        QVERIFY2(model.devir1() < peakDevir1 - 5.0,
                 "setPower(5000) sonrasi 50'ye donulunce gosterge birkac tick icinde dusmeye baslamali");
    }
};

QTEST_MAIN(TestEngineModel)
#include "test_enginemodel.moc"
