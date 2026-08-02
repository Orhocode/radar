# Pan-Tilt IR Radar Sistemi

**Türkçe** | [English](SYSTEM_DESIGN_EN.md)

Bu proje RF tabanlı klasik radar değildir; Arduino kontrollü pan-tilt mekanizma
ve Sharp analog IR mesafe sensörü ile çalışan aktif tarama sistemidir. Web
arayüzü, seri porttan gelen telemetriyi polar radar ekranına dönüştürür.

## Sistem Mimarisi

- Algılama katmanı: Sharp GP2Y0A02YK0F analog mesafe sensörü
- Hareket katmanı: MG90S pan servo, SG90S tilt servo
- Gömülü kontrol: Arduino Uno
- Yerel kullanıcı arayüzü: 16x2 I2C LCD
- Hareket izleme: MPU6050 ivmeölçer/jiroskop
- Veri kaydı: SPI SD kart modülü, `RADAR.CSV`
- Web arayüzü: Web Serial API ile USB seri port üzerinden canlı veri

## Seri Protokol

Arduino 115200 baud hızında satır bazlı JSON telemetri üretir.

Örnek:

```json
{"type":"scan","enabled":true,"pan":82,"tilt":91,"target":true,"distance_cm":64.5,"threshold_cm":120,"sd":true,"mpu":true,"imu":{"ax":120,"ay":-84,"az":16320,"gx":4,"gy":-1,"gz":2},"uptime_ms":38122}
```

Web tarafından gönderilen komutlar:

- `START`: taramayı başlatır
- `STOP`: taramayı durdurur
- `TOGGLE`: aç/kapat durumunu değiştirir
- `STATUS`: anlık telemetri ister

## Mühendislik Notları

- Servo hareketi bloklamasız zamanlama ile yapılır. Böylece LCD, sensör ve
  seri haberleşme birbirini uzun süre bekletmez.
- Buton girişi `INPUT_PULLUP` ve debounce ile okunur.
- I2C kilitlenmelerine karşı `Wire.setWireTimeout()` kullanılır.
- Servo PWM sinyali kapalı modda `detach()` ile kesilir; bu akım tüketimini
  ve titremeyi azaltır.
- Web arayüzü ham veriyi sadece listelemez; pan açısı ve mesafeyi polar
  koordinata çevirerek hedef izlerini gösterir.
- SD kaydı 1000 ms aralıkla yapılır. Bu, her sensör okumasında dosya aç/kapat
  yapmanın Uno ve SD kart üzerindeki yükünü azaltır.
- MPU6050 ham yazmaç seviyesinde okunur; ek kütüphane bağımlılığı yoktur.
  Ham değerler web tarafında `g` ve `deg/s` büyüklüklerine çevrilir.

## Gerçek Radar Yönünde Geliştirme

Daha gerçekçi radar algılaması için mevcut Sharp IR sensörü yerine şu
modüllerden biri eklenebilir:

- HB100 Doppler radar: hareket algılama, hız kestirimi için
- RCWL-0516: basit hareket algılama için
- TI IWR serisi mmWave radar: menzil, hız ve açı kestirimi için

Bu durumda web arayüzündeki polar gösterim korunur; sadece Arduino telemetri
kaynağı ve sensör işleme katmanı değişir.
