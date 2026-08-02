# Pan-Tilt IR Radar Sistemi

Arduino Uno tabanlı bu proje; pan-tilt mekanizma, Sharp IR mesafe sensörü,
MPU6050, I2C LCD ve SD kart modülünü birleştiren aktif bir tarama sistemidir.
Tarama telemetrisi USB seri portu üzerinden tarayıcıya aktarılır ve polar radar
arayüzünde canlı olarak görselleştirilir.

> Bu sistem RF tabanlı klasik radar değildir; mesafe ölçümü Sharp
> GP2Y0A02YK0F analog IR sensörü ile yapılır.

## Özellikler

- Bloklamasız pan-tilt servo kontrolü
- IR mesafe ölçümü ve eşik tabanlı hedef tespiti
- MPU6050 ivmeölçer/jiroskop telemetrisi
- 16x2 I2C LCD durum ekranı
- SD karta `RADAR.CSV` kaydı
- 115200 baud JSON seri protokolü
- Web Serial API tabanlı canlı radar arayüzü
- Donanım gerektirmeyen tarayıcı simülasyon modu

## Dizin Yapısı

```text
RadarPanTilt/       Arduino firmware'i
web/                Web Serial radar arayüzü
SYSTEM_DESIGN.md    Sistem mimarisi ve protokol ayrıntıları
Pan_Tilt_IR_Radar_Raporu_Duzenli.pdf
                    Proje raporu
```

## Proje Raporu

[Pan-Tilt IR Radar Proje Raporu](Pan_Tilt_IR_Radar_Raporu_Duzenli.pdf)

## Firmware Kurulumu

Gerekli temel bileşenler:

- Arduino Uno
- Sharp GP2Y0A02YK0F IR mesafe sensörü
- MG90S pan ve SG90S tilt servo
- MPU6050
- 16x2 I2C LCD (`0x3F`)
- SPI SD kart modülü

Arduino IDE içinde şu kütüphaneleri kurun:

- `SdFat`
- `Servo`
- `LiquidCrystal_I2C`

Ardından `RadarPanTilt/RadarPanTilt.ino` dosyasını Arduino Uno'ya yükleyin.
Servo motorları Arduino'nun 5 V pininden beslemeyin; uygun harici besleme
kullanın ve Arduino ile ortak GND oluşturun.

## Web Arayüzü

Web Serial API için Chrome veya Edge kullanın. Arayüzü yerel bir HTTP sunucusu
üzerinden açmak için:

```powershell
cd web
python -m http.server 8000
```

Tarayıcıda `http://localhost:8000` adresini açın, **Bağlan** düğmesine basın ve
Arduino'nun seri portunu seçin. Donanım olmadan arayüzü incelemek için
**Simülasyon** düğmesini kullanabilirsiniz.

Sistem mimarisi, pinler, seri komutlar ve telemetri örneği için
[`SYSTEM_DESIGN.md`](SYSTEM_DESIGN.md) belgesine bakın.
