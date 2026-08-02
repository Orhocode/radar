#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>
#include <avr/io.h>
#include <math.h>
#include <string.h>

/*
  Arduino Uno Pan-Tilt IR Radar

  Donanim baglantilari:
  - MG90S pan servo sinyal  -> D9
  - SG90S tilt servo sinyal -> D6
  - Sharp GP2Y0A02YK0F Vo   -> A0
  - Ac/Kapat butonu         -> D2 ile GND arasi
  - SD kart CS              -> D4
  - SD kart MOSI/MISO/SCK   -> D11/D12/D13
  - MPU6050 SDA/SCL         -> A4/A5, adres: 0x68
  - I2C LCD SDA/SCL         -> A4/A5, adres: 0x3F

  Onemli:
  - Servo motorlari Arduino 5V pininden besleme. 4'lu pil yatagi +6V servo
    beslemesi icin uygundur; Arduino GND ile pil/servo GND ortak olmalidir.
  - GP2Y0A02YK0F pratik olcum araligi yaklasik 20-150 cm'dir.
*/

const byte PAN_SERVO_PIN = 9;
const byte TILT_SERVO_PIN = 6;
const byte IR_SENSOR_PIN = A0;
const byte RADAR_BUTTON_PIN = 2;
const byte SD_CS_PIN = 4;
const byte SPI_SS_PIN = 10;

const byte LCD_ADDR = 0x3F;
const byte MPU6050_ADDR = 0x68;
const byte LCD_COLS = 16;
const byte LCD_ROWS = 2;

const unsigned long SERIAL_BAUD = 115200;

// Mekanik yapina gore bu sinirlari degistir. 0-180 yerine sinirli tarama,
// servo disli ve kablo zorlanmasini azaltir.
const int PAN_MIN_DEG = 45;
const int PAN_MAX_DEG = 135;
const int PAN_STEP_DEG = 1;

// Tilt ekseni bilincli olarak kucuk acida gezdiriliyor.
const int TILT_MIN_DEG = 84;
const int TILT_MAX_DEG = 96;
const int TILT_STEP_DEG = 1;
const byte TILT_UPDATE_EVERY_N_PAN_STEPS = 8;

const unsigned int SERVO_UPDATE_MS = 55;
const unsigned int SENSOR_UPDATE_MS = 90;
const unsigned int LCD_UPDATE_MS = 400;
const unsigned int SD_LOG_UPDATE_MS = 1000;
const unsigned int SD_RETRY_MS = 3000;
const unsigned int BUTTON_DEBOUNCE_MS = 35;

// Bu esikten yakin ve sensor araliginda olan hedef LCD'de "NESNE" olarak yazilir.
const float MIN_VALID_CM = 20.0;
const float MAX_VALID_CM = 150.0;
const float DETECT_THRESHOLD_CM = 120.0;
const unsigned int VCC_WARNING_MV = 4650;

Servo panServo;
Servo tiltServo;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
SdFat sd;

int panAngle = PAN_MIN_DEG;
int panDirection = 1;
int tiltAngle = 90;
int tiltDirection = 1;
byte panStepCounter = 0;

unsigned long lastLcdUpdate = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastServoUpdate = 0;

float lastDistanceCm = NAN;
bool lastTargetDetected = false;
byte resetFlags = 0;
byte lcdHeartbeat = 0;

bool sdReady = false;
bool mpuReady = false;
unsigned long lastSdLogUpdate = 0;
unsigned long lastSdRetryMs = 0;
unsigned int lastVccMv = 0;

bool radarEnabled = true;
bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastButtonChangeMs = 0;

char serialCommandBuffer[12];
byte serialCommandIndex = 0;

struct ImuData {
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t temp;
  int16_t gx;
  int16_t gy;
  int16_t gz;
};

ImuData imu = {0, 0, 0, 0, 0, 0, 0};

float readSharpDistanceCm();
int readAnalogAverage(byte pin, byte sampleCount);
void handleButton();
void handleSerialCommands();
void processSerialCommand(const char *command);
void initMpu6050();
bool readMpu6050(ImuData *data);
int16_t readInt16();
void initSdCard();
void writeCsvHeaderIfNeeded();
void logScanToSd();
unsigned int readVccMv();
void setRadarEnabled(bool enabled);
void updatePanTilt();
void updateLcd(float distanceCm, bool targetDetected);
void sendTelemetry();
const char *resetReasonText();

void setup() {
  resetFlags = MCUSR;
  MCUSR = 0;

  Serial.begin(SERIAL_BAUD);
  Serial.println(F("{\"type\":\"boot\",\"name\":\"PanTiltIRRadar\",\"baud\":115200}"));

  pinMode(RADAR_BUTTON_PIN, INPUT_PULLUP);

  panServo.attach(PAN_SERVO_PIN);
  tiltServo.attach(TILT_SERVO_PIN);

  panServo.write(panAngle);
  tiltServo.write(tiltAngle);

  Wire.begin();

  // Servo akim darbeleri veya zayif kablo nedeniyle I2C bus kilitlenirse
  // Wire sonsuza kadar beklemesin; bus resetlenip sistem akmaya devam etsin.
  Wire.setWireTimeout(25000, true);

  initMpu6050();
  initSdCard();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pan-Tilt Radar");
  lcd.setCursor(0, 1);
  lcd.print("Reset:");
  lcd.print(resetReasonText());
  delay(1500);
  lcd.clear();
}

void loop() {
  unsigned long now = millis();

  handleButton();
  handleSerialCommands();

  if (radarEnabled && now - lastServoUpdate >= SERVO_UPDATE_MS) {
    updatePanTilt();
    lastServoUpdate = now;
  }

  if (radarEnabled && now - lastSensorUpdate >= SENSOR_UPDATE_MS) {
    lastDistanceCm = readSharpDistanceCm();
    lastVccMv = readVccMv();
    if (mpuReady) {
      mpuReady = readMpu6050(&imu);
    }
    lastTargetDetected = !isnan(lastDistanceCm) && lastDistanceCm <= DETECT_THRESHOLD_CM;
    sendTelemetry();
    lastSensorUpdate = now;
  }

  if (radarEnabled && sdReady && now - lastSdLogUpdate >= SD_LOG_UPDATE_MS) {
    logScanToSd();
    lastSdLogUpdate = now;
  }

  if (!sdReady && now - lastSdRetryMs >= SD_RETRY_MS) {
    initSdCard();
    lastSdRetryMs = now;
  }

  if (now - lastLcdUpdate >= LCD_UPDATE_MS) {
    updateLcd(lastDistanceCm, lastTargetDetected);
    lastLcdUpdate = now;
  }
}

void handleButton() {
  bool reading = digitalRead(RADAR_BUTTON_PIN);
  unsigned long now = millis();

  if (reading != lastButtonReading) {
    lastButtonChangeMs = now;
    lastButtonReading = reading;
  }

  if ((now - lastButtonChangeMs) >= BUTTON_DEBOUNCE_MS && reading != stableButtonState) {
    stableButtonState = reading;

    // INPUT_PULLUP kullanildigi icin butona basilinca pin LOW olur.
    if (stableButtonState == LOW) {
      setRadarEnabled(!radarEnabled);
    }
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char incoming = Serial.read();

    if (incoming == '\n' || incoming == '\r') {
      if (serialCommandIndex > 0) {
        serialCommandBuffer[serialCommandIndex] = '\0';
        processSerialCommand(serialCommandBuffer);
        serialCommandIndex = 0;
      }
    } else if (serialCommandIndex < sizeof(serialCommandBuffer) - 1) {
      serialCommandBuffer[serialCommandIndex++] = incoming;
    } else {
      serialCommandIndex = 0;
    }
  }
}

void processSerialCommand(const char *command) {
  if (strcmp(command, "START") == 0 || strcmp(command, "1") == 0) {
    setRadarEnabled(true);
  } else if (strcmp(command, "STOP") == 0 || strcmp(command, "0") == 0) {
    setRadarEnabled(false);
  } else if (strcmp(command, "TOGGLE") == 0 || strcmp(command, "T") == 0) {
    setRadarEnabled(!radarEnabled);
  } else if (strcmp(command, "STATUS") == 0 || strcmp(command, "?") == 0) {
    sendTelemetry();
  }
}

void initMpu6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  mpuReady = (Wire.endTransmission() == 0);

  if (!mpuReady) {
    return;
  }

  // Ivme: +/-2g, jiroskop: +/-250 deg/s. Baslangic icin en hassas araliklar.
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission();
}

bool readMpu6050(ImuData *data) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(MPU6050_ADDR, (byte)14) != 14) {
    return false;
  }

  data->ax = readInt16();
  data->ay = readInt16();
  data->az = readInt16();
  data->temp = readInt16();
  data->gx = readInt16();
  data->gy = readInt16();
  data->gz = readInt16();
  return true;
}

int16_t readInt16() {
  byte highByte = Wire.read();
  byte lowByte = Wire.read();
  return (int16_t)((highByte << 8) | lowByte);
}

void initSdCard() {
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(SPI_SS_PIN, OUTPUT);
  digitalWrite(SPI_SS_PIN, HIGH);

  // Yeni kart FAT olarak bicimlendirilmis. SdFat sinifi FAT16/FAT32
  // kartlar icin Uno'da daha hafif ve yeterlidir.
  // 2 MHz breadboard/jumper kablolarda 4 MHz'e gore daha kararli olabilir.
  sdReady = sd.begin(SD_CS_PIN, SD_SCK_MHZ(2));

  if (sdReady) {
    writeCsvHeaderIfNeeded();
  }
}

void writeCsvHeaderIfNeeded() {
  if (sd.exists("RADAR.CSV")) {
    return;
  }

  File file = sd.open("RADAR.CSV", O_WRITE | O_CREAT | O_AT_END);
  if (!file) {
    sdReady = false;
    return;
  }

  file.println(F("uptime_ms,enabled,pan_deg,tilt_deg,target,distance_cm,vcc_mv,ax,ay,az,gx,gy,gz"));
  file.close();
}

void logScanToSd() {
  File file = sd.open("RADAR.CSV", O_WRITE | O_CREAT | O_AT_END);
  if (!file) {
    sdReady = false;
    return;
  }

  file.print(millis());
  file.print(',');
  file.print(radarEnabled ? 1 : 0);
  file.print(',');
  file.print(panAngle);
  file.print(',');
  file.print(tiltAngle);
  file.print(',');
  file.print(lastTargetDetected ? 1 : 0);
  file.print(',');

  if (isnan(lastDistanceCm)) {
    file.print(F("nan"));
  } else {
    file.print(lastDistanceCm, 1);
  }

  file.print(',');
  file.print(lastVccMv);
  file.print(',');
  file.print(imu.ax);
  file.print(',');
  file.print(imu.ay);
  file.print(',');
  file.print(imu.az);
  file.print(',');
  file.print(imu.gx);
  file.print(',');
  file.print(imu.gy);
  file.print(',');
  file.println(imu.gz);
  file.close();
}

unsigned int readVccMv() {
  // ATmega328P: 1.1V internal reference measured against AVcc.
  // Sonuc, Arduino'nun 5V hattinin yaklasik milivolt degeridir.
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delayMicroseconds(250);
  ADCSRA |= _BV(ADSC);

  while (bit_is_set(ADCSRA, ADSC)) {
  }

  unsigned int adc = ADC;
  if (adc == 0) {
    return 0;
  }

  return (unsigned int)(1125300L / adc);
}

void setRadarEnabled(bool enabled) {
  radarEnabled = enabled;

  if (radarEnabled) {
    if (!panServo.attached()) {
      panServo.attach(PAN_SERVO_PIN);
    }

    if (!tiltServo.attached()) {
      tiltServo.attach(TILT_SERVO_PIN);
    }

    panServo.write(panAngle);
    tiltServo.write(tiltAngle);
    lastSensorUpdate = millis();
    lastServoUpdate = millis();
  } else {
    // Kapali modda servo PWM sinyali kesilir; motor titremesi ve akim azalir.
    panServo.detach();
    tiltServo.detach();
    lastTargetDetected = false;
    lastDistanceCm = NAN;
  }

  lcd.clear();
  sendTelemetry();
}

void updatePanTilt() {
  panAngle += panDirection * PAN_STEP_DEG;

  if (panAngle >= PAN_MAX_DEG) {
    panAngle = PAN_MAX_DEG;
    panDirection = -1;
  } else if (panAngle <= PAN_MIN_DEG) {
    panAngle = PAN_MIN_DEG;
    panDirection = 1;
  }

  panStepCounter++;
  if (panStepCounter >= TILT_UPDATE_EVERY_N_PAN_STEPS) {
    panStepCounter = 0;
    tiltAngle += tiltDirection * TILT_STEP_DEG;

    if (tiltAngle >= TILT_MAX_DEG) {
      tiltAngle = TILT_MAX_DEG;
      tiltDirection = -1;
    } else if (tiltAngle <= TILT_MIN_DEG) {
      tiltAngle = TILT_MIN_DEG;
      tiltDirection = 1;
    }
  }

  panServo.write(panAngle);
  tiltServo.write(tiltAngle);
}

float readSharpDistanceCm() {
  int adc = readAnalogAverage(IR_SENSOR_PIN, 9);

  // Arduino Uno ADC: 0-1023 -> 0-5V. GP2Y0A02YK0F icin ampirik model.
  // Model ozellikle 20-150 cm bandinda anlamlidir; band disinda NaN dondurulur.
  float voltage = adc * (5.0 / 1023.0);
  if (voltage < 0.35) {
    return NAN;
  }

  float distanceCm = 61.573 * pow(voltage, -1.1068);
  if (distanceCm < MIN_VALID_CM || distanceCm > MAX_VALID_CM) {
    return NAN;
  }

  return distanceCm;
}

int readAnalogAverage(byte pin, byte sampleCount) {
  unsigned int total = 0;

  for (byte i = 0; i < sampleCount; i++) {
    total += analogRead(pin);
    delayMicroseconds(900);
  }

  return total / sampleCount;
}

void updateLcd(float distanceCm, bool targetDetected) {
  char line[LCD_COLS + 1];
  char distanceText[4];
  const char heartbeatChars[] = "|/-\\";

  lcd.setCursor(0, 0);

  if (radarEnabled && lastVccMv > 0 && lastVccMv < VCC_WARNING_MV) {
    snprintf(line, sizeof(line), "GUC DUSUK %4dmV", lastVccMv);
  } else if (!radarEnabled) {
    snprintf(line, sizeof(line), "RADAR KAPALI  %c", heartbeatChars[lcdHeartbeat]);
  } else if (targetDetected) {
    snprintf(line, sizeof(line), "NESNE GORULDU %c", heartbeatChars[lcdHeartbeat]);
  } else {
    snprintf(line, sizeof(line), "SISTEM TEMIZ  %c", heartbeatChars[lcdHeartbeat]);
  }
  lcd.print(line);

  for (byte i = strlen(line); i < LCD_COLS; i++) {
    lcd.print(" ");
  }

  if (!radarEnabled) {
    snprintf(distanceText, sizeof(distanceText), "--");
  } else if (isnan(distanceCm)) {
    snprintf(distanceText, sizeof(distanceText), "--");
  } else {
    int roundedDistance = (int)(distanceCm + 0.5);
    snprintf(distanceText, sizeof(distanceText), "%3d", roundedDistance);
  }

  snprintf(line, sizeof(line), "P%3d T%3d %3scm", panAngle, tiltAngle, distanceText);
  lcd.setCursor(0, 1);
  lcd.print(line);

  // Onceki uzun yazilardan kalan karakterleri temizle.
  for (byte i = strlen(line); i < LCD_COLS; i++) {
    lcd.print(" ");
  }

  lcdHeartbeat = (lcdHeartbeat + 1) % 4;
}

void sendTelemetry() {
  Serial.print(F("{\"type\":\"scan\",\"enabled\":"));
  Serial.print(radarEnabled ? F("true") : F("false"));
  Serial.print(F(",\"pan\":"));
  Serial.print(panAngle);
  Serial.print(F(",\"tilt\":"));
  Serial.print(tiltAngle);
  Serial.print(F(",\"target\":"));
  Serial.print(lastTargetDetected ? F("true") : F("false"));
  Serial.print(F(",\"distance_cm\":"));

  if (isnan(lastDistanceCm)) {
    Serial.print(F("null"));
  } else {
    Serial.print(lastDistanceCm, 1);
  }

  Serial.print(F(",\"threshold_cm\":"));
  Serial.print(DETECT_THRESHOLD_CM, 0);
  Serial.print(F(",\"vcc_mv\":"));
  Serial.print(lastVccMv);
  Serial.print(F(",\"sd\":"));
  Serial.print(sdReady ? F("true") : F("false"));
  Serial.print(F(",\"mpu\":"));
  Serial.print(mpuReady ? F("true") : F("false"));
  Serial.print(F(",\"imu\":{\"ax\":"));
  Serial.print(imu.ax);
  Serial.print(F(",\"ay\":"));
  Serial.print(imu.ay);
  Serial.print(F(",\"az\":"));
  Serial.print(imu.az);
  Serial.print(F(",\"gx\":"));
  Serial.print(imu.gx);
  Serial.print(F(",\"gy\":"));
  Serial.print(imu.gy);
  Serial.print(F(",\"gz\":"));
  Serial.print(imu.gz);
  Serial.print(F("}"));
  Serial.print(F(",\"uptime_ms\":"));
  Serial.print(millis());
  Serial.println(F("}"));
}

const char *resetReasonText() {
  if (resetFlags & _BV(BORF)) {
    return "BOR";
  }

  if (resetFlags & _BV(WDRF)) {
    return "WDT";
  }

  if (resetFlags & _BV(EXTRF)) {
    return "EXT";
  }

  if (resetFlags & _BV(PORF)) {
    return "POR";
  }

  return "UNK";
}
