# Pan-Tilt IR Radar Sistemi

Arduino Uno tabanli bu proje; pan-tilt mekanizma, Sharp IR mesafe sensoru,
MPU6050, I2C LCD ve SD kart modulunu birlestiren aktif bir tarama sistemidir.
Tarama telemetrisi USB seri portu uzerinden tarayiciya aktarilir ve polar radar
arayuzunde canli olarak gorsellestirilir.

> Bu sistem RF tabanli klasik radar degildir; mesafe olcumu Sharp
> GP2Y0A02YK0F analog IR sensoru ile yapilir.

## Ozellikler

- Bloklamasiz pan-tilt servo kontrolu
- IR mesafe olcumu ve esik tabanli hedef tespiti
- MPU6050 ivmeolcer/jiroskop telemetrisi
- 16x2 I2C LCD durum ekrani
- SD karta `RADAR.CSV` kaydi
- 115200 baud JSON seri protokolu
- Web Serial API tabanli canli radar arayuzu
- Donanim gerektirmeyen tarayici simulasyon modu

## Dizin Yapisi

```text
RadarPanTilt/       Arduino firmware'i
web/                Web Serial radar arayuzu
thesis_template/    LaTeX rapor kaynaklari ve gorseller
SYSTEM_DESIGN.md    Sistem mimarisi ve protokol ayrintilari
```

Derlenmis rapor PDF'leri, gecici render dosyalari ve LaTeX ara ciktilari depoya
dahil edilmez.

## Firmware Kurulumu

Gerekli temel bilesenler:

- Arduino Uno
- Sharp GP2Y0A02YK0F IR mesafe sensoru
- MG90S pan ve SG90S tilt servo
- MPU6050
- 16x2 I2C LCD (`0x3F`)
- SPI SD kart modulu

Arduino IDE icinde su kutuphaneleri kurun:

- `SdFat`
- `Servo`
- `LiquidCrystal_I2C`

Ardindan `RadarPanTilt/RadarPanTilt.ino` dosyasini Arduino Uno'ya yukleyin.
Servo motorlari Arduino'nun 5 V pininden beslemeyin; uygun harici besleme
kullanin ve Arduino ile ortak GND olusturun.

## Web Arayuzu

Web Serial API icin Chrome veya Edge kullanin. Arayuzu yerel bir HTTP sunucusu
uzerinden acmak icin:

```powershell
cd web
python -m http.server 8000
```

Tarayicida `http://localhost:8000` adresini acin, **Baglan** dugmesine basin ve
Arduino'nun seri portunu secin. Donanim olmadan arayuzu incelemek icin
**Simulasyon** dugmesini kullanabilirsiniz.

Sistem mimarisi, pinler, seri komutlar ve telemetri ornegi icin
[`SYSTEM_DESIGN.md`](SYSTEM_DESIGN.md) belgesine bakin.
