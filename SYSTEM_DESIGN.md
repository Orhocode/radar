# Pan-Tilt IR Radar Sistemi

Bu proje RF tabanli klasik radar degildir; Arduino kontrollu pan-tilt mekanizma
ve Sharp analog IR mesafe sensoru ile calisan aktif tarama sistemidir. Web
arayuzu, seri porttan gelen telemetriyi polar radar ekranina donusturur.

## Sistem Mimarisi

- Algilama katmani: Sharp GP2Y0A02YK0F analog mesafe sensoru
- Hareket katmani: MG90S pan servo, SG90S tilt servo
- Gomulu kontrol: Arduino Uno
- Yerel kullanici arayuzu: 16x2 I2C LCD
- Hareket izleme: MPU6050 ivmeolcer/jiroskop
- Veri kaydi: SPI SD kart modulu, `RADAR.CSV`
- Web arayuzu: Web Serial API ile USB seri port uzerinden canli veri

## Seri Protokol

Arduino 115200 baud hizinda satir bazli JSON telemetri uretir.

Ornek:

```json
{"type":"scan","enabled":true,"pan":82,"tilt":91,"target":true,"distance_cm":64.5,"threshold_cm":120,"sd":true,"mpu":true,"imu":{"ax":120,"ay":-84,"az":16320,"gx":4,"gy":-1,"gz":2},"uptime_ms":38122}
```

Web tarafindan gonderilen komutlar:

- `START`: taramayi baslatir
- `STOP`: taramayi durdurur
- `TOGGLE`: ac/kapat durumunu degistirir
- `STATUS`: anlik telemetri ister

## Muhendislik Notlari

- Servo hareketi bloklamasiz zamanlama ile yapilir. Boylece LCD, sensor ve
  seri haberlesme birbirini uzun sure bekletmez.
- Buton girisi `INPUT_PULLUP` ve debounce ile okunur.
- I2C kilitlenmelerine karsi `Wire.setWireTimeout()` kullanilir.
- Servo PWM sinyali kapali modda `detach()` ile kesilir; bu akim tuketimini
  ve titremeyi azaltir.
- Web arayuzu ham veriyi sadece listelemez; pan acisi ve mesafeyi polar
  koordinata cevirerek hedef izlerini gosterir.
- SD kaydi 250 ms aralikla yapilir. Bu, her sensor okumasinda dosya ac/kapat
  yapmanin Uno ve SD kart uzerindeki yukunu azaltir.
- MPU6050 ham register seviyesinde okunur; ek kutuphane bagimliligi yoktur.
  Ham degerler web tarafinda `g` ve `deg/s` buyukluklerine cevrilir.

## Gercek Radar Yonunde Gelistirme

Daha gercekci radar algilamasi icin mevcut Sharp IR sensor yerine su
modullerden biri eklenebilir:

- HB100 Doppler radar: hareket algilama, hiz kestirimi icin
- RCWL-0516: basit hareket algilama icin
- TI IWR serisi mmWave radar: menzil, hiz ve aci kestirimi icin

Bu durumda web arayuzundeki polar gosterim korunur; sadece Arduino telemetri
kaynagi ve sensor isleme katmani degisir.
