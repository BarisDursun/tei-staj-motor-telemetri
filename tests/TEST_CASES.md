# Test Paketi — Dokümantasyon

| | |
|---|---|
| **Kapsam** | `engine_core.h` (fizik/domain modeli) ve `EngineModel` (Qt köprüsü) |
| **Hazırlayan** | Kişi 2 — Test |
| **İlgili dokümanlar** | [../docs/gereksinimler.md](../docs/gereksinimler.md), [../docs/test-parametre-referans-degerleri.md](../docs/test-parametre-referans-degerleri.md) |
| **Son çalıştırma** | `test_engine_core`: 80/80 CHECK PASS · `test_enginemodel`: 15/15 QtTest PASS (0 FAIL) |

> Bu dosya, `gereksinimler.md` onayından sonra baştan yazıldı. Önceki sürüm, Kişi 1'in gerçek `EngineModel` implementasyonundan önceki bir **placeholder API**'yi (`temperature()`, `alarmState()`, `setTemperatureForTest()`...) test ediyordu ve artık şu anki kodla derlenmiyordu — bkz. eski `docs/test-parametre-referans-degerleri.md` §7.

## 1. İki Ayrı Test Dosyası, İki Farklı Amaç

| Dosya | Framework | Neden ayrı |
|---|---|---|
| `test_engine_core.cpp` | Yok — elle yazılmış `CHECK`/`CHECK_NEAR` makroları, sadece `<cstdio>`/`<cmath>` | `engine_core.h`'nin gerçekten Qt'den bağımsız olduğunu **kanıtlamak** için (NFR-05). Hiçbir Qt kütüphanesine bağlanmıyor. |
| `test_enginemodel.cpp` | QtTest (`Qt5::Test`) | `EngineModel`, Qt sinyal/slot sistemine bağlı bir köprü katmanı — `QSignalSpy` gibi Qt'ye özgü araçlar gerekiyor (NFR-06). |

## 2. `test_engine_core.cpp` — Kategoriler

| Kategori | Kapsadığı FR | Ne test ediyor |
|---|---|---|
| A. WearFactor / yaş sınırları | FR-09, NFR-04 | Taze motor → 0 yıpranma; **negatif yaş 0'a sabitleniyor** (bkz. `gereksinimler.md` NFR-04 kararı); çok büyük yaş 1'e yaklaşır ama geçmez. |
| B. GetMaintenanceStatus üç seviye | FR-17 | `w=0.45` ve `w=0.70` sınırlarının hemen altı/üstünde doğru seviye (Healthy/Watch/MaintenanceRequired). |
| C. Engine_Start güç clamp | FR-24 | Negatif güç → rölanti gibi, aşırı büyük güç → tam güç gibi davranıyor. |
| D. ParamCeilings tutarlılığı | — | Taze motorda (age=0) %100 güçteki fiziksel değer, `GetParamCeilings()` ile birebir eşleşiyor. |
| E / E2 | FR-12, FR-14a | TF10000 `EvaluateAlarm()` sınır matrisi — her parametre için `warnAt-ε / warnAt / critAt-ε / critAt` (kod `>=` kullanıyor, tam eşikte alarm zaten tetiklenmeli). YağBasıncı'nın iki taraflı `bandRange` mantığı ayrıca test edildi. **Yeni eklenen YağSıcaklığı alarmı (FR-14a) dahil.** |
| F / F2 | FR-12, FR-14b | Aynısı PD170 için. **Yeni eklenen turbo Basınç (overboost) alarmı (FR-14b) dahil.** |
| G. worstOf | FR-14 | Aynı anda birden fazla parametre eşik aşarsa gösterilen seviye en kötüsü oluyor. |
| H. WearScale | FR-09 | Aynı yaştaki motorda, yıpranma etkisi rölantide düşük, tam güçte daha yüksek çıkıyor. |
| I. EGT sensör kazancı | FR-09a | STM32 dahili sıcaklık sensöründen gelen +10°C sapmanın, `EGT_SENSOR_GAIN` ile her motorun kendi EGT tabanına eklenip eklenmediği; **daha önemlisi**, aynı sapmanın TF10000 ve PD170'de EGT aralığının (span) birebir **aynı yüzdesini** kaydırdığının matematiksel doğrulaması. |

## 3. `test_enginemodel.cpp` — Kategoriler

| Kategori | Kapsadığı FR | Ne test ediyor |
|---|---|---|
| A. Motor seçimi / filo | FR-04, FR-25 | `selectFleetEngine`'in `m_engineFamily`'ye göre doğru sınıfı yarattığı (PD170 filo regresyon testi); geçersiz motor adı/`fleetId`'nin önceki motoru **bozmadan** reddedildiği. |
| B. Simülasyon / spool | FR-10, FR-11 | Motor durunca parametrelerin kademeli düştüğü (anlık sıfırlanmadığı); motor değişiminde önceki test/yıpranma durumunun sızmadığı. |
| C. Alarm start-inhibit | FR-13 | Spool-up'ın hemen başında (yağ basıncı ataleti oturmadan) alarmın zorunlu olarak Normal kaldığı. |
| D. Bakım teşhisi | FR-15, FR-16, FR-17 | Test edilmeden "HENÜZ TEST EDİLMEDİ" gösterildiği; farklı yaşlardaki filo motorlarının (0/5/10 yıl) doğru SAĞLIKLI/İZLENMELİ/BAKIM GEREKLİ metnini ürettiği; `m_tested` geçişinde `maintenanceStatusChanged`'in tetiklendiği. |
| E. wearNotes | FR-18, FR-19 | `docs/test-parametre-referans-degerleri.md`'deki hesapla birebir örtüşen bir senaryo (PD170, 10 yaş, tam güç): sadece %10'u aşan iki sapmanın (Titreşim +57%, Yağ Basıncı -14%), büyükten küçüğe sıralı biçimde listelendiği; %10 altındakilerin (EGT, Yağ Sıcaklığı, Yakıt) filtrelendiği. |
| F. Girdi sağlamlığı | FR-24, NFR-04 | `setPower()`'a aşırı büyük bir hedef (5000) verilip sonra makul bir değere (50) dönüldüğünde, göstergenin birkaç tick içinde tepki verdiği — clamp eklenmeden önceki "eski değerde uzun süre takılı kalma" davranışının artık oluşmadığı. |

## 4. Kapsam Dışı / Bilinen Test Boşlukları

Şeffaflık için: her şey otomatik testli değil.

- **`EmbeddedTempSensor` (`embeddedtempsensor.cpp`)** hiç unit test edilmiyor — `QSerialPort` üzerinden gerçek donanıma bağlanıyor, bunu izole test etmek bir mock/fake seri port gerektirir (kapsam dışı bırakıldı). Şu an sadece manuel olarak doğrulandı: kart takılı değilken uygulama çökmeden varsayılan 25°C'de kalıyor (bkz. `docs/gereksinimler.md` FR-09a).
- **FR-20/21/22/23 (QML/UI davranışları)** otomatik test edilmiyor — Qt Quick Test ayrı bir efor gerektirir, şu an için manuel/gözle doğrulama listesinde (`docs/gereksinimler.md` §4 Kabul Kriterleri).
- **NFR-01/02 (performans), NFR-11 (renk kodlaması)** de aynı şekilde manuel doğrulama kapsamında — otomatik bir performans/görsel regresyon testi yok.

## 5. Nasıl Çalıştırılır

```bash
"C:\Qt\Tools\CMake_64\bin\cmake.exe" --build "C:\Users\brsdr\Desktop\EngineTelemetry\build"
"C:\Qt\Tools\CMake_64\bin\ctest.exe" --test-dir "C:\Users\brsdr\Desktop\EngineTelemetry\build" --output-on-failure
```

`ctest` çalışmadan önce Qt/MinGW DLL dizinlerinin `PATH`'te olması gerekiyor (`C:\Qt\5.15.2\mingw81_64\bin`, `C:\Qt\Tools\mingw810_64\bin`) — aksi halde `test_enginemodel` DLL bulunamadığı için `0xc0000135` hatasıyla çöker (framework hatası değil, ortam eksikliği).

**Bilinen ortam kısıtlaması:** `test_enginemodel.exe`'nin QtTest çıktısı bu shell ortamında konsola düşmüyor (eski `TEST_CASES.md`'de de aynı not vardı) — süreç yine de doğru çıkış koduyla (0=hepsi PASS) dönüyor ve `ctest` bunu doğru okuyor. Ayrıntılı PASS/FAIL dökümü görmek gerekirse: `test_enginemodel.exe -o sonuc.txt,txt` ile dosyaya yazdırılabilir, ya da Qt Creator üzerinden çalıştırılabilir.

`test_engine_core.exe` ise sıradan bir konsol programı olduğu için çıktısı doğrudan terminalde görünür.
