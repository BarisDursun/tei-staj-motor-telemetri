# EngineTelemetry — Fonksiyonel ve Fonksiyonel Olmayan Gereksinimler

| | |
|---|---|
| **Durum** | Onaylandı — testler yazıldı ve geçiyor (bkz. `tests/TEST_CASES.md`) |
| **Hazırlayan** | Kişi 2 — Test |
| **Amaç** | Test yazımına başlamadan önce "ne test ediliyor" ve "neden" sorularını tek bir referans dokümanda netleştirmek |
| **İlgili dosya** | [test-parametre-referans-degerleri.md](test-parametre-referans-degerleri.md) — bu dokümandaki sayısal sınır değerlerin kaynağı |

## 1. Kapsam

Bu doküman şu an kod tabanında var olan davranışı (mevcut `engine_core.h`, `enginemodel.h/.cpp`, `main.cpp`, `qml/Main.qml`, `qml/TrendGraph.qml`) sistematik olarak gereksinimlere döker. Amaç yeni özellik önermek değil — **var olan davranışı** onaylanabilir, test edilebilir maddeler haline getirmek, ve testler yazılırken "bu bug mu, kasıtlı mı" tartışmasını en aza indirmek.

Onaydan sonraki adım: her FR/NFR maddesi bir veya birden fazla test senaryosuna dönüşecek (unit test — `engine_core.h` saf C++ olduğu için Qt bağımsız test edilebilir; entegrasyon testi — `EngineModel` katmanı için QtTest/`QSignalSpy`).

## 2. Fonksiyonel Gereksinimler (FR)

### 2.1 Motor Seçimi ve Filo Yönetimi

| ID | Gereksinim |
|---|---|
| FR-01 | Kullanıcı iki motor ailesinden birini seçebilmeli: **TF10000** veya **PD170**. |
| FR-02 | `selectEngine(name)` çağrıldığında sistem, seçilen aileyi (`m_engineFamily`) hatırlamalı; bu bilgi daha sonra filo seçiminde doğru alt sınıfı örneklemek için kullanılmalı. |
| FR-03 | `fleetEngines()` seçili aile için sabit yaş listesini (`kFleet`) döndürmeli; her kayıt bir `fleetId` ve `ageYears` içermeli. |
| FR-04 | `selectFleetEngine(fleetId)` çağrıldığında sistem, `m_engineFamily`'ye göre doğru alt sınıfı (`TF10000` ya da `PD170`) o filo kaydının yaşıyla örneklemeli. Yanlış aile örneklenmesi (ör. PD170 seçiliyken TF10000 örneklenmesi) kritik bir hatadır. |
| FR-05 | Filo listesi, motor **test edilmeden önce** herhangi bir bakım durumu (SAĞLIKLI/İZLENMELİ/BAKIM GEREKLİ) göstermemeli — bkz. FR-15. |

### 2.2 Simülasyon / Fizik Modeli

| ID | Gereksinim |
|---|---|
| FR-06 | `setPower(percent)` ile ayarlanan hedef güç (`m_targetPower`), gerçek fiziksel güce (`m_actualPower`) anlık sıçramayla değil, motora özgü rise/fall atalet süreleriyle (`SpoolProfile`) yaklaşmalı. |
| FR-07 | Her `simulationTick()` (200 ms) çağrısında tüm sekiz parametre (Devir1, Devir2, Basınç, EGT, Yakıt, YağBasıncı, YağSıcaklığı, Titreşim) motor tipine özgü formülle (`Engine_Start`) yeniden hesaplanmalı. |
| FR-08 | Hesaplanan değerlere gürültü (jitter) eklenmeli, ancak jitter sonrası değer `ParamCeilings`'i (motorun o güçteki fiziksel tavanı) aşmamalı (`qMin` ile kırpılmalı). |
| FR-09 | Motor yaşı arttıkça (`ageYears`), `WearFactor()` (üstel doygunluk eğrisi `1 - e^(-t/8)`) ile ölçeklenen bir yıpranma etkisi parametrelere yansımalı; bu etki güç seviyesine göre de ölçeklenmeli (`WearScale(power)` — rölantide az, tam güçte çok). |
| FR-09a | Gömülü ekipten (STM32, bkz. `Gömülü.cpp.txt`) UART üzerinden gelen ham sıcaklık, **ortam sıcaklığı olarak değil**, `EGT_SENSOR_GAIN` katsayısıyla motora özgü oranlanarak doğrudan EGT'ye yansıtılmalı. Katsayı, her motorun kendi EGT aralığından (span = tam güç EGT − rölanti EGT) türetilir (`gain = span/100`) — böylece aynı fiziksel sensör sapması, TF10000 ve PD170'de EGT aralığının **aynı yüzdesini** kaydırır (bkz. `test-parametre-referans-degerleri.md` §2/§3). Sensör bağlı değilse 25°C referans değeri kullanılır (katkı sıfır, mevcut davranışla birebir aynı). |
| FR-10 | Motor durdurulduğunda (`stopEngine`) tüm parametreler motora özgü fall (düşüş) sürelerine göre kademeli olarak sıfıra/dinlenme değerine dönmeli, anlık sıfırlanmamalı. |
| FR-11 | Motor değiştirildiğinde (`resetSimulationStateFor`) önceki motorun simülasyon durumu (atalet çarpanları, `m_tested`, alarm, wear notları) tamamen sıfırlanmalı — bir motordan kalan durum yeni seçilen motora sızmamalı. |

### 2.3 Alarm Sistemi

| ID | Gereksinim |
|---|---|
| FR-12 | Sistem her tick'te motora özgü `EvaluateAlarm()` sonucuna göre `alarmLevel`'i (Normal/Warning/Critical) güncellemeli. |
| FR-13 | Alarm değerlendirmesi ve gösterimi, motor gerçekten "yerleşik" (settled) duruma gelene kadar (yağ basıncı ataletinin `%97`'sine ulaşana kadar, `m_factorYagBasinci >= 0.97`) devreye girmemeli — spool-up geçişlerinde sahte (false-positive) alarm üretilmemeli. |
| FR-14 | Birden fazla parametre aynı anda eşik aşarsa, gösterilen alarm seviyesi bunların **en kötüsü** olmalı (`worstOf`). |
| FR-14a | TF10000'de YağSıcaklığı da (yağ film kaybı/yatak hasarı riski nedeniyle) `EvaluateAlarm()`'a dahil olmalı: `warnAt=100°C`, `critAt=110°C`. |
| FR-14b | PD170'de turbo Basıncı (overboost, kritik bir turbo-dizel arıza modu) `EvaluateAlarm()`'a dahil olmalı: `warnAt=2.3 bar`, `critAt=2.5 bar`. |
| FR-14c | Her iki motorda da Yakıt debisi **kasıtlı olarak** anlık alarm dışı bırakılmalı — tek başına yüksekliği tehlikeli değil, sapması ancak trend (`wearNotes`/ECTM) üzerinden anlamlı. |

### 2.4 Bakım Teşhis Sistemi (Maintenance)

| ID | Gereksinim |
|---|---|
| FR-15 | Bakım durumu (`maintenanceStatusText`) ve yıpranma notları (`wearNotes`), motor **en az bir kez fiilen çalıştırılıp test edilene kadar** (`m_tested == false` iken) kullanıcıya gösterilmemeli; bunun yerine "HENÜZ TEST EDİLMEDİ" gösterilmeli. Bu, "test etmeden teşhis koyma" — profesyonellik açısından ekibin özellikle önem verdiği bir kuraldır. |
| FR-16 | `m_tested`, alarm gösterimiyle aynı eşiğe (`m_factorYagBasinci >= 0.97`) bağlı olarak `true` olmalı — motor bir kez bu eşiğe ulaştıktan sonra test edilmiş sayılmalı ve bu durumdan geri dönülmemeli (motor durdurulsa bile geçmiş test geçerliliğini korumalı — motor değişiminde sıfırlanır, bkz. FR-11). |
| FR-17 | Bakım durumu, `GetMaintenanceStatus()`'a göre üç seviyeden biri olmalı: SAĞLIKLI (`w < 0.45`), İZLENMELİ (`0.45 <= w < 0.70`), BAKIM GEREKLİ (`w >= 0.70`). |
| FR-18 | Yıpranma notları (`wearNotes`), sadece **%10'dan fazla** sapan parametreleri, sapma büyüklüğüne göre sıralı listelemeli — küçük/önemsiz sapmalar kullanıcıyı gereksiz yere uyarmamalı. |
| FR-19 | Yıpranma notları, motor test edildikten sonra **anlık güce göre canlı** yeniden hesaplanmalı (rölantide görülen yıpranma yüzdesi ile tam güçte görülen aynı olmamalı — bkz. `test-parametre-referans-degerleri.md` §5 WearScale gerekçesi). |

### 2.5 Kullanıcı Arayüzü

| ID | Gereksinim |
|---|---|
| FR-20 | Motor seçim ekranı her iki motor için de "gerçek TEI ürünü" rozetini ve gösterilen verilerin simülasyon/demo amaçlı olduğuna dair uyarıyı göstermeli. |
| FR-21 | "MOTORU BAŞLAT" kontrolü, motor zaten çalışırken (`isEngineOn == true`) devre dışı olmalı — aynı motor ikinci kez "başlatılamamalı". |
| FR-22 | Gösterge (gauge) ve parametre kutuları, EICAS/ECAM renk kuralına uymalı: cyan = etiket, beyaz = normal değer, amber = uyarı, kırmızı = kritik. |
| FR-23 | Trend grafikleri sekmesi (`Grafikler`), izlenen sekiz parametrenin her biri için ayrı bir kaydırmalı `TrendGraph` göstermeli, eşik referans çizgileriyle birlikte. |

### 2.6 Girdi Doğrulama / Sağlamlık

| ID | Gereksinim |
|---|---|
| FR-24 | `setPower()`'a 0–100 aralığı dışında bir değer verilirse (negatif veya >100), `m_targetPower` doğrudan `setPower()` içinde `qBound(0.0, x, 100.0)` ile kırpılmalı — `Engine_Start`'taki 0–1 clamp'e ek olarak, `m_actualPower`'ın sınırsız sürünmesi (UI'da görünmeyen ama iç durumu kirleten bir sürüklenme) burada engellenmeli. |
| FR-25 | Geçersiz/tanımsız bir `fleetId` veya motor adıyla çağrılan `selectFleetEngine`/`selectEngine`, sistemi çökertmemeli **ve** önceki motor durumunu bozmamalı: her iki fonksiyon da geçersiz girdide erken `return` ile sessizce reddetmeli, `currentEngine` asla `nullptr` bırakılmamalı (aksi halde `simulationTick()` no-op'a düşer, UI son gördüğü değerlerde donar ve kullanıcıya hiçbir hata gitmez — bu, karar öncesi `selectEngine`'in gerçek davranışıydı). |

## 3. Fonksiyonel Olmayan Gereksinimler (NFR)

### 3.1 Performans

| ID | Gereksinim |
|---|---|
| NFR-01 | Simülasyon tick'i 200 ms periyotta sürekli çalışmalı; bir tick'in işlenmesi bir sonraki tick'in zamanlamasını geciktirmemeli (UI donması yaşanmamalı). |
| NFR-02 | Canvas tabanlı gösterge/grafik çizimleri (`onPaint`), her tick'te gereksiz yeniden çizim yapmadan sadece `requestPaint()` ile tetiklenmeli. |

### 3.2 Doğruluk / Güvenilirlik

| ID | Gereksinim |
|---|---|
| NFR-03 | Aynı girdi dizisi (güç profili + yaş + STM sensör sıcaklığı) verildiğinde simülasyon **deterministik** sonuç üretmeli — jitter dışında rastgelelik olmamalı; jitter'ın kendisi de sınırlı bir aralıkta olmalı (ceiling'i aşmamalı, bkz. FR-08). |
| NFR-04 | Sistem, geçersiz girdilerde çökmemeli. Proje genelinde tutarlı iki kural benimsendi: **sayısal/fiziksel girdiler clamp edilir** (güç → `setPower()`'da 0–100, yaş → `WearFactor()`'da negatif değer 0'a sabitlenir), **kategorik/kimlik girdileri (motor adı, `fleetId`) sessizce reddedilir** ve önceki durum korunur (`selectEngine`/`selectFleetEngine`'de erken `return`, `currentEngine` asla `nullptr` bırakılmaz). |

### 3.3 Test Edilebilirlik

| ID | Gereksinim |
|---|---|
| NFR-05 | `engine_core.h` (fizik/domain modeli) Qt'den tamamen bağımsız kalmalı — bu sayede saf C++ unit testleriyle (QtTest'siz, hatta CMake'te Qt5::Test bağımlılığı olmadan) test edilebilmeli. |
| NFR-06 | `EngineModel` (Qt köprü katmanı), `QSignalSpy` ile NOTIFY sinyallerinin doğru zamanda/doğru sayıda tetiklendiği doğrulanabilecek şekilde tasarlanmalı. |
| NFR-07 | Test edilebilirlik için üretim kodunda eklenen yardımcılar (varsa) test dışı davranışı etkilememeli. |

### 3.4 Sürdürülebilirlik / Bakım Kolaylığı

| ID | Gereksinim |
|---|---|
| NFR-08 | Yeni bir motor ailesi eklenmesi (üçüncü bir `Engine` alt sınıfı), mevcut `TF10000`/`PD170` mantığına dokunmadan, sadece yeni bir sınıf + `EngineModel`'e bir `if/else` dalı eklenerek yapılabilmeli (mevcut polimorfik tasarım bunu zaten destekliyor — bu NFR o tasarımın korunmasını garanti altına alıyor). |
| NFR-09 | Kod içi Türkçe yorumlar "ne" değil "neden" odaklı olmalı (ör. neden bu eşik, neden bu birim varsayımı) — ekip üyelerinin mentöre kod anlatabilmesi bu okunabilirliğe bağlı. |

### 3.5 Kullanılabilirlik

| ID | Gereksinim |
|---|---|
| NFR-10 | Kullanıcı (mentör/değerlendirici dahil), motoru hiç çalıştırmadan bakım durumu göremediği için yanıltıcı/erken bir teşhis riski taşımamalı (bkz. FR-15) — bu profesyonellik açısından ekibin özellikle vurguladığı bir gereksinimdir. |
| NFR-11 | Uyarı/kritik renk kodlaması (amber/kırmızı) endüstri standardıyla (EICAS/ECAM) tutarlı olmalı, ekstra öğrenme gerektirmemeli. |

### 3.6 Taşınabilirlik

| ID | Gereksinim |
|---|---|
| NFR-12 | Uygulama Qt 5.15.2 + MinGW 8.1.0 + CMake/Ninja ile Windows 11'de derlenip çalışmalı (mevcut geliştirme ortamı); `QT_QUICK_BACKEND=software` GPU sürücü kaynaklı titremeyi önlemek için korunmalı. |

## 4. Kabul Kriterleri (Genel)

- Her FR/NFR maddesi en az bir otomatik teste (unit veya entegrasyon) bağlanabilmeli; bağlanamayanlar (ör. NFR-11 gibi görsel/öznel maddeler) manuel/gözle doğrulama listesine alınmalı.
- `docs/test-parametre-referans-degerleri.md`'deki sınır değerleri, ilgili testlerin veri-odaklı (`QTest::addColumn`) senaryolarında birebir kullanılmalı.
- §2.6 ve §3.2'deki davranışlar (FR-24/25, NFR-04) artık karara bağlandı ve kodda uygulandı (`enginemodel.cpp::setPower/selectEngine`, `engine_core.h::WearFactor`) — testler bunları "olması gereken" olarak doğrulamalı, eski `tests/TEST_CASES.md`'deki gibi sadece "o an ne olduğunu" belgelememeli.

## 5. Sonraki Adım

Bu doküman onaylandıktan sonra:

1. `tests/` klasörü, gerçek `EngineModel`/`Engine` API'sine göre **baştan** yazılacak (mevcut `test_enginemodel.cpp` artık şu anki kodla derlenmiyor, bkz. `test-parametre-referans-degerleri.md` §7).
2. Öncelik sırası: (a) `engine_core.h` için Qt'siz saf unit testler (FR-06/07/08/09/12/17/18/19 + tüm alarm sınır matrisi), (b) `EngineModel` için QtTest entegrasyon testleri (FR-01–05, 11, 13, 15, 16, NFR-04/06).
3. Tüm açık sorular karara bağlandı ve koda işlendi: `test-parametre-referans-degerleri.md` §4'teki 4 madde (titreşim/basınç birimi, TF10000 YağSıcaklığı ve PD170 Basınç alarmları → `engine_core.h`, FR-14a/b/c) ve FR-25/NFR-04 (geçersiz girdi davranışı → `enginemodel.cpp` + `engine_core.h::WearFactor`). Testler artık hepsini "olması gereken" davranış olarak doğrulamalı.

---

**Onay:** Doküman onaylandı, testler yazıldı (`tests/test_engine_core.cpp`: 80/80, `tests/test_enginemodel.cpp`: 15/15 PASS). Sonradan eklenen FR-09a (STM32 EGT sensör entegrasyonu) da dahil tüm maddeler koda ve testlere yansıtıldı.
