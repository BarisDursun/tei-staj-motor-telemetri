# Test Parametre Referans Değerleri

| | |
|---|---|
| **Kapsam** | `TF10000` ve `PD170` (`engine_core.h`) parametrelerinin test sınır (boundary) değerleri |
| **Amaç** | Testleri yazmadan önce her parametre için gerçekçi min/nominal/max/kritik değerleri tek yerde toplamak |
| **Yöntem** | Koddaki mevcut sabitler, gerçek dünya (TEI ürün sayfaları, Wikipedia, ECTM/EICAS endüstri literatürü, dizel havacılık motoru verileri) referanslarıyla karşılaştırıldı |
| **Hazırlayan** | Kişi 2 — Test |

## 1. Neden Bu Dosya Var

Testleri (özellikle sınır değer / equivalence class testlerini) yazarken "bu değer gerçekçi mi, bu eşik anlamlı mı" sorusuna her seferinde tekrar araştırma yapmamak için: koddaki her sabit, bulabildiğimiz en yakın gerçek dünya referansıyla yan yana kondu. İki motor da farklı gerekçelerle ele alınıyor:

- **TF10000** — TEI'nin gerçek ürünü (TF6000 ailesi, 6.000 lbf kuru / 10.000 lbf art yakıcılı) kamuya sadece itki/boyut verisi açıklıyor; EGT, yağ basıncı, titreşim gibi iç limitler üretici sertifikasyon verisi olduğu için gizli. Bu yüzden simülasyondaki eşikler TEI'nin gerçek limitleri **değil**, genel turbofan endüstri pratiğinden (ECTM, EICAS, GE/RR/PW bakım dokümanları) türetilmiş gerçekçi mertebelerdir.
- **PD170** — TEI'nin gerçek, kamuya açık spesifikasyonu olan ürünü (turbo-dizel). Nominal değerler doğrudan üretici verisiyle karşılaştırılabiliyor.

## 2. TF10000 — Turbofan (jenerik, endüstri pratiğine dayalı)

| Parametre | Kod: Rölanti (0%) | Kod: Tam Güç (100%, taze motor) | Kod: Uyarı | Kod: Kritik | Gerçek Dünya Referans | Değerlendirme |
|---|---|---|---|---|---|---|
| Devir1 (N1, %) | 35 | 100 | ≥95 | ≥105 | Continuous 60–100%, take-off 100–103%, redline ~103% | Tutarlı — kod eşiği gerçek N1 redline mertebesine yakın. |
| Devir2 (N2, %) | 65 | 100 | ≥95 | ≥105 | Continuous 101–103%, min 95%, max 103% | Tutarlı mertebe. |
| Basınç (birimsiz basınç oranı, EPR benzeri) | 2.0 | 20.0 | — (alarmda yok) | — (alarmda yok) | Karşılaştırılabilir gerçek veri yok | §4 kararı: birim netleşti (EPR benzeri oran); alarma bağlanmadı — TF10000'de düşük/yüksek basınç oranı zaten Devir1/Devir2/EGT üzerinden dolaylı yakalanıyor, ayrı bir uçuş-güvenliği eşiği olduğuna dair veri yok. |
| EGT (°C) | ~480 | ~950 (wear dahil taban) | ≥775 | ≥850 | Normal seyir ~500–600°C; yüksek performans/askeri turbofanlarda tam güçte 900–1000°C mertebesi görülebiliyor | Tutarlı. |
| Yakıt (birim tanımsız, "pph" varsayımı) | 250 | 3100 | — (kasıtlı alarmda yok, §4) | — (kasıtlı alarmda yok, §4) | Doğrudan karşılaştırma yok (iç/göreli birim) | Sadece izleniyor, trend (`wearNotes`) üzerinden anlamlı. |
| YağBasıncı (psi varsayımı) | 40 | 70 | ≤35 / ≥75 | ≤30 / ≥80 | GE: 10–30 psi @55%N2 → 37–80 psi @112%N2; RR: min 25–35 psi | Tutarlı — GE tipi jenerik turbin aralığına yakın. |
| YağSıcaklığı (°C) | 60 | 120 | ≥100 | ≥110 | Genel turbin yağ sıcaklığı normal aralığı ~60–120°C mertebesinde | Tutarlı — §4 karar sonrası alarm bandı eklendi. |
| Titreşim (birimsiz, dahili göreli skor) | 1.5 | 6.0 | ≥4.0 | ≥5.0 | Genel havacılıkta "<0.5 ips normal" eşiği yaygın referans | Gerçek IPS ölçeğiyle karşılaştırılamaz (birim farklı) — §4 kararı: gerçek fiziksel birim değil, dahili göreli şiddet skoru olarak tanımlandı. |

## 3. PD170 — TEI Turbo-Dizel (gerçek ürün, kamuya açık spesifikasyon)

**Gerçek ürün verisi (TEI / Wikipedia / Unmanned Systems Technology):** 172 hp (±2) @ 2.200 rpm anma gücü, 120 hp @ 30.000 ft / 170 hp @ 20.000 ft, 4 silindirli sıralı, 4 zamanlı, sıvı soğutmalı, çift kademeli turboşarj, 2.100 cc, 16:1 sıkıştırma oranı, kuru ağırlık 210 kg, Jet-A1/JP-8/EN 590 yakıt uyumlu, tam güçte 25 L/h yakıt tüketimi, FADEC (TEI'nin kendi yazılımı), MTBF >1000 saat.

| Parametre | Kod: Rölanti (0%) | Kod: Tam Güç (100%, taze motor) | Kod: Uyarı | Kod: Kritik | Gerçek Dünya Referans | Değerlendirme |
|---|---|---|---|---|---|---|
| Devir1 (RPM) | 1000 | 2800 (tavan) | ≥2500 | ≥2800 | Anma gücü 2.200 rpm'de veriliyor | Tutarlı — kod tavanı (2800) anma devrinin üstünde, overspeed koruma payı olarak makul. |
| Devir2 (soğutma suyu sıcaklığı, °C — **RPM değil**) | 70 | 105 (tavan) | ≥100 | ≥120 | Dizel motor soğutma suyu normal aralığı ~65–95°C, uyarı ~100–105°C, kritik >110–115°C | Tutarlı. |
| Basınç (turbo boost, bar) | 1.0 | 2.6 (tavan) | ≥2.3 | ≥2.5 | Küçük turbo-dizel motorlarda tipik boost 1.5–2.5 bar (mutlak) | Mertebe tutarlı — §4 karar sonrası overboost alarm bandı eklendi. |
| EGT (°C) | 250 | 600 (tavan) | ≥480 | ≥550 | Küçük turbo-dizellerde normal seyir 300–500°C, tam yükte 550–650°C mertebesi | Tutarlı. |
| Yakıt (L/h varsayımı) | 4 | 36 (tavan) | — (kasıtlı alarmda yok, §4) | — (kasıtlı alarmda yok, §4) | Üretici verisi: tam güçte 25 L/h | Kod tavanı (36) biraz yüksek ama yıpranma payı dahil tavan değeri olduğu için mertebe makul; 25 L/h aralık içinde kalıyor. |
| YağBasıncı (bar varsayımı) | 2.5 | 5.5 (tavan) | ≤2.0 / ≥5.8 | ≤1.5 / ≥6.0 | Dizel motorlarda tipik yağ basıncı 2–6 bar | Tutarlı. |
| YağSıcaklığı (°C) | 70 | 115 (tavan) | — (alarmda yok) | — (alarmda yok) | Dizel yağ sıcaklığı normal ~90–110°C, tavan ~120–125°C | Tutarlı, alarm mantığında kontrol edilmiyor. |
| Titreşim (birimsiz, dahili göreli skor) | 1.0 | 3.0 (tavan) | ≥2.2 | ≥2.7 | Pistonlu/dizel motor titreşimi turbin IPS ölçeğinden farklı bir referans gerektirir | TF10000 ile aynı tanım (§4), ama iç ölçek daha mütevazı (1–3) ve kendi içinde tutarlı. |

## 4. Açık Sorular — Karara Bağlandı

Aşağıdaki 4 madde, bu doküman ilk yazıldığında açık soruydu; ekip içi netleştirme beklemeden proje kapsamında karara bağlanıp `engine_core.h`'a işlendi (derleme doğrulandı). Testler artık bu kararları **beklenen davranış** olarak doğrulamalı.

1. **Titreşim birimi:** Gerçek bir fiziksel birim (IPS/mils) değil — motora özgü, birimsiz, dahili göreli şiddet skoru olarak tanımlandı ve `Engine` sınıfında `param_Titresim` alanının yanına bunu belirten bir yorum eklendi. TF10000 ve PD170 arasında doğrudan karşılaştırılamaz (ölçekleri farklı: tavan ~6 vs ~3).
2. **Basınç birimi:** TF10000'de `param_Basinc` birimsiz bir basınç oranı (EPR benzeri) olarak; PD170'de ise bar cinsinden mutlak turbo basıncı olarak tanımlandı ve ilgili `Engine_Start()` satırlarına açıklayıcı yorum eklendi.
3. **TF10000: Yakıt alarm dışı (kasıtlı), YağSıcaklığı artık alarma bağlı.** Yağ aşırı ısınması gerçek bir uçuş güvenliği riski (yağ film kaybı, yatak hasarı) olduğu için `EvaluateAlarm()`'a `bandHigh(param_YagSicakligi, 100.0f, 110.0f)` eklendi. Yakıt debisi ise bilinçli olarak alarm dışı bırakıldı: anlık yüksekliği tek başına tehlikeli değil, ancak trend (ECTM/`wearNotes`) üzerinden anlamlı — bu zaten `GetWearDeviations()` ile izleniyor.
4. **PD170: Basınç (turbo boost) artık alarma bağlı.** Gerçek bir turbo-dizelde overboost kritik bir arıza modu olduğu için `EvaluateAlarm()`'a `bandHigh(param_Basinc, 2.3f, 2.5f)` eklendi (taban tavan değeri ~2.6 bar). PD170'de Yakıt de aynı gerekçeyle (TF10000 ile tutarlı olarak) alarm dışı bırakıldı.
5. Yeni eklenen iki alarm bandı için `test-parametre-referans-degerleri.md` §5'teki sınır matrisi deseniyle testler yazılmalı: TF10000 YağSıcaklığı için 100/110°C etrafında, PD170 Basınç için 2.3/2.5 bar etrafında `warnAt-ε / warnAt / warnAt+ε / critAt-ε / critAt / critAt+ε` senaryoları.

## 5. Test Girdi Matrisi Önerisi

- **Güç seviyeleri:** 0% (rölanti), 25%, 50%, 75%, 100%, ve clamp testi için sınır-dışı girdiler (-10%, 110%) — `Engine_Start()` başında `power` 0–1 aralığına clamp ediliyor, bu davranış test edilmeli.
- **Yaş (ageYears) / filo eşleşmesi:** mevcut `kFleet` değerleri (0, ..., 20 yıl aralığında 10 kayıt) + `WearFactor()` eşik sınırları:
  - Healthy → Watch geçişi: `w = 0.45` → `1 - e^(-t/8) = 0.45` → **t ≈ 4.8 yıl**
  - Watch → MaintenanceRequired geçişi: `w = 0.70` → **t ≈ 9.63 yıl**
  - Bu iki yaş değeri (~4.8 ve ~9.63 yıl) `GetMaintenanceStatus()` sınır testleri için özel olarak eklenmeli (eşiğin hemen altı/üstü, eski `TEST_CASES.md`'deki "tam eşikte" desenine benzer şekilde).
- **STM sensör sıcaklığı (`sensorTemp`):** Ortam değil, EGT'ye motora özgü oranla (`EGT_SENSOR_GAIN`) yansıyan bir girdi — test değerleri: 25°C (referans/nötr, katkı sıfır), 35°C (+10°C, TF10000'de +47°C EGT / PD170'de +35°C EGT), -10°C (soğutma senaryosu) uç senaryoları.
- **Alarm sınır matrisi:** her alarm'a giren parametre için `warnAt - ε`, `warnAt`, `warnAt + ε`, `critAt - ε`, `critAt`, `critAt + ε` — kod `>=` kullanıyor (`bandHigh`/`bandRange`), yani tam eşik değerinde alarm zaten **tetiklenmiş** olmalı (eski placeholder testteki `>` davranışının tam tersi — bu fark testte özellikle doğrulanmalı).
- **`m_tested` / start-inhibit geçişi:** `m_factorYagBasinci >= 0.97` eşiğinin hemen altı/üstü — bakım teşhisinin gerçekten "test edilmeden önce" gizli kaldığını doğrulamak için.

## 6. Kaynakça

- [TEI-PD170 | Wikipedia](https://en.wikipedia.org/wiki/TEI-PD170)
- [TEI-PD170 | 172 hp turbodiesel engine for MALE UAVs — Unmanned Systems Technology](https://www.unmannedsystemstechnology.com/company/tei-tusas-engine-industries/tei-pd170/)
- [TEI - TEI-PD170 Turbodiesel Aviation Engine](https://www.tei.com.tr/en/products/tei-pd170)
- [TEI Delivered A Pair of PD170 Engines to Baykar Makina — Defence Turkey](https://www.defenceturkey.com/en/content/tei-delivered-a-pair-of-pd170-engines-to-baykar-makina-3262)
- [TEI - TEI-TF10000 Turbofan Engine](https://www.tei.com.tr/en/products/tei-tf10000-turbofan-engine)
- [TEI-TF6000 | Wikipedia](https://en.wikipedia.org/wiki/TEI-TF6000)
- [Internal Layout of TEI TF6000, a low bypass turbofan engine — Defence Turkey](https://defenceturkey.com/en/content/internal-layout-of-tei-tf6000-a-low-bypass-turbofan-engine-5301)
- [Gas Turbine Engine Oil System Maintenance — Aircraft Systems Tech](https://www.aircraftsystemstech.com/p/turbine-engine-oil-system-maintenance_97.html?m=1)
- [What are N1 and N2 in Aviation Turbine Engines? — Airplane Academy](https://airplaneacademy.com/what-are-n1-and-n2-in-aviation-turbine-engines/)
- [What is exhaust gas temperature in aviation? — Tsunami Air](https://tsunamiair.com/airplane/exhaust/temperature)
- [Engine Vibration Analysis — King Air Magazine](https://kingairmagazine.com/article/engine-vibration-analysis/)
- [Operating Conditions for Detroit Diesel 92 Series Engines — Diesel Pro Power](https://dieselpro.com/blog/operating-conditions-for-detroit-diesel-92-series-engines-6v92-8v92-12v92-16v92/)
- [Introduction to Engine Condition Trend Monitoring (ECTM) — SASSofia](https://sassofia.com/blog/introduction-to-engine-condition-trend-monitoring-ectm/)
- [Using Engine Condition Trend Monitoring As A Standard — Duncan Aviation](https://www.duncanaviation.aero/intelligence/using-engine-condition-trend-monitoring-as-a-standard)

## 7. Not: Mevcut `tests/` Klasörü Hakkında

`tests/test_enginemodel.cpp` ve `tests/TEST_CASES.md`, Kişi 1'in gerçek `EngineModel` implementasyonu teslim edilmeden önce yazılmış bir **placeholder API**'yi test ediyor (`temperature()`, `alarmState()`, `setTemperatureForTest()`, `resetAlarm()`, `vibration()`...). Şu anki `enginemodel.h` bu metodların hiçbirini içermiyor (`p_EGT`, `p_Devir1`, `alarmLevel`, `selectEngine`, `fleetEngines`... kullanıyor) — yani bu eski test dosyası **artık şu anki kodla derlenmiyor**. Bu dosya, gereksinimler onaylandıktan sonra yazılacak yeni test setinin gerçek API'ye göre baştan hazırlanması gerektiğini gösteriyor; şimdilik dokunulmadı.
