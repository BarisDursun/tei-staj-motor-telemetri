# EngineModel — Test Case Dokümantasyonu

| | |
|---|---|
| **Modül** | `EngineModel` (`enginemodel.h` / `enginemodel.cpp`) |
| **Test dosyası** | `tests/test_enginemodel.cpp` |
| **Framework** | QtTest (Qt 5.15.2) |
| **Hazırlayan** | Kişi 2 — Test |
| **Son çalıştırma** | 26/26 PASS, 0 FAIL, 0 SKIP |
| **Ortam** | Qt 5.15.2, MinGW 8.1.0 64-bit, Windows 11 |

## 1. Kapsam ve Amaç

Bu doküman `EngineModel` çekirdek sınıfının şu davranışlarını doğrulayan test senaryolarını tanımlar:
- Alarm eşik mantığının (`temperature > 90°C`) doğruluğu ve sınır değerlerdeki tutarlılığı
- `Q_PROPERTY` / `NOTIFY` sinyal disiplini (sinyal doğru zamanda, doğru sayıda tetikleniyor mu)
- `resetAlarm()` komutunun durum yönetimi
- Aşırı/geçersiz girdilerde çökme olup olmadığı
- `vibration` property'sinin temel doğruluğu

Test edilen `EngineModel`, Kişi 1'in gerçek implementasyonu teslim edilene kadar kullanılan **placeholder** sürümdür (`enginemodel.h/.cpp` içindeki yorum satırında belirtilmiştir). Property isimleri ve imzaları kilitlendiği için testler gerçek implementasyon geldiğinde değişiklik gerektirmeden çalışmaya devam edecektir.

## 2. Özet Tablo

| ID | Test Adı | Kategori | Durum |
|---|---|---|---|
| TC-01 | freshModelStartsBelowThreshold | A. Başlangıç durumu | ✅ PASS |
| TC-02 | alarmThreshold (12 senaryo, veri-odaklı) | B. Eşik/sınır matrisi | ✅ PASS |
| TC-03 | temperatureChangedFiresOnSet | C. Sinyal disiplini | ✅ PASS |
| TC-04 | temperatureChangedFiresEvenIfValueUnchanged | C. Sinyal disiplini | ✅ PASS ⚠️ |
| TC-05 | alarmStateChangedFiresOnlyOnTransition | C. Sinyal disiplini | ✅ PASS |
| TC-06 | alarmStateChangedFiresOnReturnToNormal | C. Sinyal disiplini | ✅ PASS |
| TC-07 | resetAlarmClearsActiveAlarm | D. resetAlarm davranışı | ✅ PASS |
| TC-08 | resetAlarmEmitsSignalWhenClearingActiveAlarm | D. resetAlarm davranışı | ✅ PASS |
| TC-09 | resetAlarmWhileStillHot_reTriggersOnNextReading | D. resetAlarm davranışı | ✅ PASS ⚠️ |
| TC-10 | extremeNegativeTemperatureDoesNotCrash | E. Uç durum/dayanıklılık | ✅ PASS ⚠️ |
| TC-11 | extremePositiveTemperatureDoesNotCrash | E. Uç durum/dayanıklılık | ✅ PASS |
| TC-12 | freshModelHasZeroVibration | F. Vibration | ✅ PASS |
| TC-13 | vibrationChangedFiresOnSet | F. Vibration | ✅ PASS |

⚠️ işaretli testler PASS oluyor ama **açık bir tasarım sorusunu belgeliyor** — bkz. [Bölüm 4](#4-testler-sırasında-bulunan-açık-tasarım-soruları).

---

## 3. Detaylı Test Senaryoları

### Kategori A — Başlangıç Durumu

#### TC-01 — freshModelStartsBelowThreshold
- **Amaç:** Yeni oluşturulan bir `EngineModel` nesnesinin varsayılan sıcaklığının (20°C) eşiğin (90°C) altında olduğunu, dolayısıyla alarmın başlangıçta kapalı geldiğini doğrulamak.
- **Ön koşul:** Yok — kurucu (constructor) dışında hiçbir işlem yapılmamış nesne.
- **Adımlar:** 1) `EngineModel model;` oluştur.
- **Beklenen sonuç:** `model.alarmState() == false`
- **Gerçek sonuç:** PASS

---

### Kategori B — Eşik / Sınır Değer Matrisi

#### TC-02 — alarmThreshold (veri-odaklı, 12 senaryo)
- **Amaç:** Alarm eşik mantığının (`temperature > 90.0`) tüm kritik sınır bölgelerinde doğru çalıştığını tek bir test fonksiyonuyla kapsamlı biçimde doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** Her senaryo için: 1) yeni `EngineModel` oluştur, 2) `setTemperatureForTest(sıcaklık)` çağır, 3) `alarmState()`'i beklenen değerle karşılaştır.
- **Test verisi:**

| Senaryo | Sıcaklık (°C) | Beklenen Alarm |
|---|---|---|
| negatif sıcaklık | -40.0 | false |
| donma noktası | 0.0 | false |
| soğuk | 20.0 | false |
| ılık | 70.0 | false |
| eşiğin hemen altı | 89.9 | false |
| eşiğin çok hemen altı | 89.999 | false |
| **tam eşikte** | 90.0 | **false** |
| eşiğin çok hemen üstü | 90.001 | true |
| eşiğin hemen üstü | 90.1 | true |
| sıcak | 105.0 | true |
| çok sıcak | 120.0 | true |
| aşırı değer | 1000.0 | true |

- **Beklenen sonuç:** Her satır için `alarmState()` tablodaki değerle eşleşir.
- **Gerçek sonuç:** PASS (12/12 satır)
- **Not:** "tam eşikte" satırı özellikle önemli — kod `>` operatörü kullanıyor (`>=` değil), yani tam 90.0'da alarm **tetiklenmemeli**. Bu test, biri yanlışlıkla `>=` yaparsa kırmızı çıkacak şekilde tasarlandı.

---

### Kategori C — Sinyal Disiplini (`NOTIFY`)

#### TC-03 — temperatureChangedFiresOnSet
- **Amaç:** `setTemperatureForTest()` çağrıldığında `temperatureChanged` sinyalinin gerçekten yayınlandığını doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** 1) `QSignalSpy` ile `temperatureChanged` sinyalini dinlemeye başla, 2) `setTemperatureForTest(30.0)` çağır.
- **Beklenen sonuç:** `spy.count() == 1`
- **Gerçek sonuç:** PASS

#### TC-04 — temperatureChangedFiresEvenIfValueUnchanged ⚠️
- **Amaç:** Sıcaklık zaten set edilmiş bir değere **tekrar aynı değerle** set edildiğinde sinyalin yine de tetiklenip tetiklenmediğini belgelemek.
- **Ön koşul:** Sıcaklık önce 50.0'a set edilmiş.
- **Adımlar:** 1) `setTemperatureForTest(50.0)`, 2) spy'ı başlat, 3) `setTemperatureForTest(50.0)` — **aynı değer**.
- **Beklenen sonuç (mevcut davranış):** `spy.count() == 1` — yani değer değişmese de sinyal atılıyor.
- **Gerçek sonuç:** PASS
- **Not:** ⚠️ Bu "doğru" davranış değil, **mevcut** davranış. Değer değişmediği halde sinyal atmak QML tarafında gereksiz binding yeniden hesaplamasına yol açar. Bkz. Bölüm 4.

#### TC-05 — alarmStateChangedFiresOnlyOnTransition
- **Amaç:** `alarmStateChanged` sinyalinin sadece gerçek bir durum **geçişinde** (false→true veya true→false) tetiklendiğini, alarm zaten aktifken tekrar yüksek sıcaklık set edilirse tekrar tetiklenmediğini doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** 1) spy başlat, 2) `setTemperatureForTest(95.0)` (false→true), 3) `setTemperatureForTest(100.0)` (true→true), 4) `setTemperatureForTest(110.0)` (true→true).
- **Beklenen sonuç:** `spy.count() == 1` (sadece ilk geçişte), `alarmState() == true`.
- **Gerçek sonuç:** PASS

#### TC-06 — alarmStateChangedFiresOnReturnToNormal
- **Amaç:** Alarm aktifken sıcaklık normale döndüğünde sinyalin tekrar tetiklendiğini doğrulamak (true→false geçişi).
- **Ön koşul:** Sıcaklık 95.0'a set edilmiş, alarm aktif.
- **Adımlar:** 1) spy başlat, 2) `setTemperatureForTest(50.0)`.
- **Beklenen sonuç:** `spy.count() == 1`, `alarmState() == false`.
- **Gerçek sonuç:** PASS

---

### Kategori D — `resetAlarm()` Davranışı

#### TC-07 — resetAlarmClearsActiveAlarm
- **Amaç:** `resetAlarm()` çağrıldığında aktif alarmın gerçekten kapandığını doğrulamak.
- **Ön koşul:** Sıcaklık 95.0, alarm aktif.
- **Adımlar:** 1) `resetAlarm()` çağır.
- **Beklenen sonuç:** `alarmState() == false`
- **Gerçek sonuç:** PASS

#### TC-08 — resetAlarmEmitsSignalWhenClearingActiveAlarm
- **Amaç:** `resetAlarm()`'ın alarmı kapatırken `alarmStateChanged` sinyalini de yayınladığını doğrulamak (QML/web tarafının haberdar olması için şart).
- **Ön koşul:** Sıcaklık 95.0, alarm aktif.
- **Adımlar:** 1) spy başlat, 2) `resetAlarm()` çağır.
- **Beklenen sonuç:** `spy.count() == 1`
- **Gerçek sonuç:** PASS

#### TC-09 — resetAlarmWhileStillHot_reTriggersOnNextReading ⚠️
- **Amaç:** Sıcaklık hâlâ eşiğin üstündeyken `resetAlarm()` çağrılırsa, bir sonraki okumada alarmın davranışını belgelemek.
- **Ön koşul:** Yok.
- **Adımlar:** 1) `setTemperatureForTest(95.0)`, 2) `resetAlarm()` → `alarmState() == false` olduğunu doğrula, 3) `setTemperatureForTest(95.0)` — **aynı yüksek değer, "bir sonraki okuma" simülasyonu**.
- **Beklenen sonuç (mevcut davranış):** `alarmState() == true` — yani alarm hemen geri geliyor.
- **Gerçek sonuç:** PASS
- **Not:** ⚠️ Reset, sıcaklığı düşürmüyor — sadece bayrağı temizliyor. Sıcaklık hâlâ yüksekse bir sonraki `simulateStep()`/okumada alarm otomatik geri geliyor. Kullanıcı deneyimi açısından tartışmalı bir davranış, bkz. Bölüm 4.

---

### Kategori E — Uç Durum / Dayanıklılık

#### TC-10 — extremeNegativeTemperatureDoesNotCrash ⚠️
- **Amaç:** Aşırı negatif bir sıcaklık değeri verildiğinde sistemin çökmediğini ve değeri **olduğu gibi** kabul ettiğini doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** 1) `setTemperatureForTest(-1.0e9)`.
- **Beklenen sonuç:** Çökme yok, `alarmState() == false`, `temperature() == -1.0e9` (sınırlama/clamp yok).
- **Gerçek sonuç:** PASS
- **Not:** ⚠️ `simulateStep()` içindeki `qBound(0.0, ..., 120.0)` sınırlaması burada **uygulanmıyor** — test girişi ham olarak kabul ediliyor. Bkz. Bölüm 4.

#### TC-11 — extremePositiveTemperatureDoesNotCrash
- **Amaç:** Aşırı pozitif bir sıcaklık değerinde de sistemin çökmediğini doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** 1) `setTemperatureForTest(1.0e9)`.
- **Beklenen sonuç:** Çökme yok, `alarmState() == true`, `temperature() == 1.0e9`.
- **Gerçek sonuç:** PASS

---

### Kategori F — Vibration

#### TC-12 — freshModelHasZeroVibration
- **Amaç:** Yeni nesnenin varsayılan `vibration` değerinin 0.0 olduğunu doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** 1) `EngineModel model;` oluştur.
- **Beklenen sonuç:** `vibration() == 0.0`
- **Gerçek sonuç:** PASS

#### TC-13 — vibrationChangedFiresOnSet
- **Amaç:** `setVibrationForTest()` hem değeri güncelliyor hem `vibrationChanged` sinyalini tetikliyor mu, doğrulamak.
- **Ön koşul:** Yok.
- **Adımlar:** 1) spy başlat, 2) `setVibrationForTest(2.5)`.
- **Beklenen sonuç:** `spy.count() == 1`, `vibration() == 2.5`.
- **Gerçek sonuç:** PASS
- **Not:** `setVibrationForTest()` test edilebilirlik için Kişi 2 tarafından eklendi (orijinal placeholder'da yoktu) — `setTemperatureForTest()` ile aynı kalıp.

---

## 4. Testler Sırasında Bulunan Açık Tasarım Soruları

Bu 3 madde "test başarısız" değil — **testler geçiyor çünkü mevcut davranışı doğru şekilde belgeliyorlar**. Ama davranışın kendisi tartışmaya açık, Kişi 1 ile netleştirilmeli:

| # | Bulgu | İlgili Test | Soru |
|---|---|---|---|
| 1 | Değer değişmese bile `NOTIFY` sinyali koşulsuz tetikleniyor | TC-04 | `if (value == m_temperature) return;` gibi bir koruma eklenmeli mi? |
| 2 | `setTemperatureForTest` hiçbir sınırlama (clamp) yapmıyor | TC-10 | Test girişi kasıtlı olarak ham mı kalmalı, yoksa `simulateStep()`'teki `qBound` mantığı burada da mı uygulanmalı? |
| 3 | `resetAlarm()` sonrası sıcaklık hâlâ yüksekse alarm anında geri geliyor | TC-09 | İstenen davranış bu mu, yoksa sıcaklık gerçekten eşiğin altına düşene kadar alarm sessiz mi kalmalı? |

## 5. Nasıl Çalıştırılır

1. Qt Creator'da `EngineTelemetry` projesini aç.
2. Sol alt köşedeki hedef seçiciden **`test_enginemodel`**'i seç.
3. `Ctrl+B` (derle) → `Ctrl+R` (çalıştır).
4. **Application Output** panelinde sonuçları gör (`PASS`/`FAIL` satırları).

Komut satırından: `build/tests/test_enginemodel.exe` (not: bu ortamda CLI çıktısı konsola düşmüyor, Qt Creator üzerinden çalıştırmak güvenilir sonuç için gerekli).
