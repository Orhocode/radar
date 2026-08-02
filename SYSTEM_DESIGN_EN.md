# Pan-Tilt IR Radar System

[Türkçe](SYSTEM_DESIGN.md) | **English**

This project is not a conventional RF radar. It is an active scanning system
built around an Arduino-controlled pan-tilt mechanism and a Sharp analog IR
distance sensor. The web interface converts serial telemetry into a polar
radar display.

## System Architecture

- Sensing layer: Sharp GP2Y0A02YK0F analog distance sensor
- Motion layer: MG90S pan servo and SG90S tilt servo
- Embedded controller: Arduino Uno
- Local user interface: 16x2 I2C LCD
- Motion monitoring: MPU6050 accelerometer and gyroscope
- Data logging: SPI SD card module using `RADAR.CSV`
- Web interface: live data over USB serial through the Web Serial API

## Serial Protocol

The Arduino emits line-oriented JSON telemetry at 115200 baud.

Example:

```json
{"type":"scan","enabled":true,"pan":82,"tilt":91,"target":true,"distance_cm":64.5,"threshold_cm":120,"sd":true,"mpu":true,"imu":{"ax":120,"ay":-84,"az":16320,"gx":4,"gy":-1,"gz":2},"uptime_ms":38122}
```

Commands sent by the web interface:

- `START`: starts scanning
- `STOP`: stops scanning
- `TOGGLE`: toggles the enabled state
- `STATUS`: requests the current telemetry state

## Engineering Notes

- Servo motion uses non-blocking scheduling so the LCD, sensors, and serial
  communication do not stall each other for long periods.
- The button input uses `INPUT_PULLUP` with debounce filtering.
- `Wire.setWireTimeout()` protects against an indefinitely locked I2C bus.
- Servo PWM is disabled with `detach()` while scanning is off, reducing power
  consumption and mechanical jitter.
- The web interface converts pan angle and distance measurements into polar
  coordinates instead of only presenting raw values.
- SD logging runs at 1000 ms intervals to reduce file open/close overhead on
  the Arduino Uno and the SD card.
- The MPU6050 is read at register level without an additional library. The web
  application converts raw values into `g` and `deg/s` units.

## Moving Toward a Real Radar

The Sharp IR sensor can be replaced with one of the following modules for a
more realistic radar sensing architecture:

- HB100 Doppler radar for motion detection and speed estimation
- RCWL-0516 for basic motion detection
- TI IWR-series mmWave radar for range, velocity, and angle estimation

The polar display can remain unchanged in these configurations. Only the
Arduino telemetry source and sensor-processing layer need to be replaced.
