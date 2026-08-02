# Pan-Tilt IR Radar System

[Türkçe](README.md) | **English**

This Arduino Uno-based project combines a pan-tilt mechanism, a Sharp IR
distance sensor, an MPU6050, an I2C LCD, and an SD card module into an active
scanning system. Scan telemetry is transferred to the browser over a USB
serial connection and visualized on a live polar radar display.

> This is not a conventional RF radar system. Distance measurements are
> provided by a Sharp GP2Y0A02YK0F analog IR sensor.

## Features

- Non-blocking pan-tilt servo control
- IR distance measurement and threshold-based target detection
- MPU6050 accelerometer and gyroscope telemetry
- 16x2 I2C LCD status display
- Data logging to `RADAR.CSV` on an SD card
- Line-oriented JSON protocol at 115200 baud
- Live Web Serial API radar interface
- Browser simulation mode that does not require hardware

## Repository Structure

```text
RadarPanTilt/       Arduino firmware
web/                Web Serial radar interface
SYSTEM_DESIGN_EN.md System architecture and protocol details
Pan_Tilt_IR_Radar_Raporu_Duzenli.pdf
                    Project report (Turkish)
```

## Project Report

[Pan-Tilt IR Radar Project Report (Turkish)](Pan_Tilt_IR_Radar_Raporu_Duzenli.pdf)

## Firmware Setup

Required hardware:

- Arduino Uno
- Sharp GP2Y0A02YK0F IR distance sensor
- MG90S pan servo and SG90S tilt servo
- MPU6050
- 16x2 I2C LCD (`0x3F`)
- SPI SD card module

Install the following libraries in the Arduino IDE:

- `SdFat`
- `Servo`
- `LiquidCrystal_I2C`

Upload `RadarPanTilt/RadarPanTilt.ino` to the Arduino Uno. Do not power the
servos from the Arduino 5 V pin. Use a suitable external supply and connect
the supply ground to the Arduino ground.

## Web Interface

Use Chrome or Edge for Web Serial API support. Start a local HTTP server:

```powershell
cd web
python -m http.server 8000
```

Open `http://localhost:8000`, click **Connect**, and select the Arduino serial
port. Click **Simulation** to test the interface without hardware.

See [`SYSTEM_DESIGN_EN.md`](SYSTEM_DESIGN_EN.md) for the system architecture,
pin assignments, serial commands, and an example telemetry packet.
