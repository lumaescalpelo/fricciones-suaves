#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ============================================================
// CONFIGURACION OLED
// ============================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

// ============================================================
// PINES
// ============================================================
#define PULSE_PIN      34
#define EMG_PIN        35
#define TOUCH_PIN      4
#define HUMIDITY_PIN   32
#define NEOPIXEL_PIN   27
#define NUM_PIXELS     8

// ============================================================
// ESTRUCTURA DE COLOR
// ============================================================
struct ColorRGB {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// ============================================================
// OBJETOS
// ============================================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_NeoPixel strip(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_MPU6050 mpu;

// ============================================================
// CONFIGURACION GENERAL
// ============================================================
const bool TOUCH_ACTIVE_HIGH = true;

// ============================================================
// INTERVALOS DE LECTURA / ACTUALIZACION
// ============================================================
const unsigned long pulseReadIntervalMs    = 4;
const unsigned long emgReadIntervalMs      = 5;
const unsigned long touchReadIntervalMs    = 10;
const unsigned long imuReadIntervalMs      = 20;
const unsigned long humidityReadIntervalMs = 200;
const unsigned long displayUpdateMs        = 120;
const unsigned long ledUpdateMs            = 25;
const unsigned long beatTimeoutMs          = 2500;

// ============================================================
// ESTADO PULSO
// ============================================================
int rawPulse = 0;
int bpm = 0;

int thresholdHigh = 2300;
int thresholdLow  = 2100;

bool pulseStateHigh = false;
unsigned long lastBeatTime = 0;

// ============================================================
// ESTADO EMG
// ============================================================
int rawEMG = 0;

// ============================================================
// ESTADO TOUCH
// ============================================================
bool touchDetected = false;

// ============================================================
// ESTADO IMU
// ============================================================
float ax = 0.0f;
float ay = 0.0f;
float az = 0.0f;

float gx = 0.0f;
float gy = 0.0f;
float gz = 0.0f;

bool imuOk = false;

// ============================================================
// ESTADO HUMEDAD
// ============================================================
int rawHumidity = 0;
int humidityPercent = 0;

// ============================================================
// ESTADO ANIMACION
// ============================================================
unsigned long animationStep = 0;

// ============================================================
// TIMERS
// ============================================================
unsigned long lastPulseReadTime    = 0;
unsigned long lastEmgReadTime      = 0;
unsigned long lastTouchReadTime    = 0;
unsigned long lastImuReadTime      = 0;
unsigned long lastHumidityReadTime = 0;
unsigned long lastDisplayTime      = 0;
unsigned long lastLedTime          = 0;

// ============================================================
// PROTOTIPOS
// ============================================================
void updatePulseSensor(unsigned long now);
void updateEMGSensor();
void updateTouchSensor();
void updateIMUSensor();
void updateHumiditySensor();
void updateDisplay();
void updateTransGlitterAnimation();

uint8_t brightenComponent(uint8_t value, uint8_t amount);
uint8_t fadeComponent(uint8_t value, uint8_t amount);
uint8_t softenToPastelSubtle(uint8_t value);

ColorRGB lerpColor(const ColorRGB& a, const ColorRGB& b, float t);
ColorRGB transCloudPalette(float phase);

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) {
      // Si falla la OLED, quedarse aqui
    }
  }

  display.setRotation(2);
  display.clearDisplay();
  display.display();

  analogReadResolution(12);
  analogSetPinAttenuation(PULSE_PIN, ADC_11db);
  analogSetPinAttenuation(EMG_PIN, ADC_11db);
  analogSetPinAttenuation(HUMIDITY_PIN, ADC_11db);

  strip.begin();
  strip.show();
  strip.setBrightness(110);

  randomSeed(micros());

  imuOk = mpu.begin();
  if (imuOk) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }
}

// ============================================================
// LOOP PRINCIPAL NO BLOQUEANTE
// ============================================================
void loop() {
  unsigned long now = millis();

  if (now - lastPulseReadTime >= pulseReadIntervalMs) {
    lastPulseReadTime = now;
    updatePulseSensor(now);
  }

  if (now - lastEmgReadTime >= emgReadIntervalMs) {
    lastEmgReadTime = now;
    updateEMGSensor();
  }

  if (now - lastTouchReadTime >= touchReadIntervalMs) {
    lastTouchReadTime = now;
    updateTouchSensor();
  }

  if (imuOk && (now - lastImuReadTime >= imuReadIntervalMs)) {
    lastImuReadTime = now;
    updateIMUSensor();
  }

  if (now - lastHumidityReadTime >= humidityReadIntervalMs) {
    lastHumidityReadTime = now;
    updateHumiditySensor();
  }

  if (lastBeatTime != 0 && (now - lastBeatTime > beatTimeoutMs)) {
    bpm = 0;
  }

  if (now - lastLedTime >= ledUpdateMs) {
    lastLedTime = now;
    updateTransGlitterAnimation();
  }

  if (now - lastDisplayTime >= displayUpdateMs) {
    lastDisplayTime = now;
    updateDisplay();
  }
}

// ============================================================
// SENSOR DE PULSO
// ============================================================
void updatePulseSensor(unsigned long now) {
  rawPulse = analogRead(PULSE_PIN);

  if (!pulseStateHigh && rawPulse >= thresholdHigh) {
    pulseStateHigh = true;

    if (lastBeatTime > 0) {
      unsigned long interval = now - lastBeatTime;

      if (interval >= 300 && interval <= 2000) {
        bpm = 60000 / interval;
      }
    }

    lastBeatTime = now;
  }

  if (pulseStateHigh && rawPulse <= thresholdLow) {
    pulseStateHigh = false;
  }
}

// ============================================================
// SENSOR EMG
// ============================================================
void updateEMGSensor() {
  rawEMG = analogRead(EMG_PIN);
}

// ============================================================
// SENSOR TOUCH
// ============================================================
void updateTouchSensor() {
  int rawTouch = digitalRead(TOUCH_PIN);

  if (TOUCH_ACTIVE_HIGH) {
    touchDetected = (rawTouch == HIGH);
  } else {
    touchDetected = (rawTouch == LOW);
  }
}

// ============================================================
// SENSOR IMU
// ============================================================
void updateIMUSensor() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  ax = a.acceleration.x;
  ay = a.acceleration.y;
  az = a.acceleration.z;

  gx = g.gyro.x;
  gy = g.gyro.y;
  gz = g.gyro.z;
}

// ============================================================
// SENSOR HUMEDAD
// ============================================================
void updateHumiditySensor() {
  rawHumidity = analogRead(HUMIDITY_PIN);

  // Ajusta estos valores segun tu calibracion real
  humidityPercent = map(rawHumidity, 1200, 3000, 0, 100);

  if (humidityPercent < 0) humidityPercent = 0;
  if (humidityPercent > 100) humidityPercent = 100;
}

// ============================================================
// OLED
// ============================================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Título
  display.setCursor(0, 0);
  display.println("FRICCIONES SUAVES");

  // Columnas
  const int x1 = 0;
  const int x2 = 64;
  const int x3 = 96;

  // Filas
  const int y1 = 12;
  const int y2 = 22;
  const int y3 = 30;
  const int y4 = 38;
  const int y5 = 48;

  // Fila 1: BPM, Touch, Humedad
  display.setCursor(x1, y1);
  display.print("B:");
  display.print(bpm);
  if (rawPulse < 50) {
    display.print("*");
  }

  display.setCursor(x2, y1);
  display.print("T:");
  display.print(touchDetected ? "O" : "X");

  display.setCursor(x3, y1);
  display.print("H:");
  display.print(humidityPercent);
  display.print("%");

  // Fila 2
  display.setCursor(x1, y2);
  display.print("AX:");
  display.print(ax, 1);

  display.setCursor(x2, y2);
  display.print("GX:");
  display.print(gx, 1);

  // Fila 3
  display.setCursor(x1, y3);
  display.print("AY:");
  display.print(ay, 1);

  display.setCursor(x2, y3);
  display.print("GY:");
  display.print(gy, 1);

  // Fila 4
  display.setCursor(x1, y4);
  display.print("AZ:");
  display.print(az, 1);

  display.setCursor(x2, y4);
  display.print("GZ:");
  display.print(gz, 1);

  // Fila 5: EMG crudo
  display.setCursor(x1, y5);
  display.print("E:");
  display.print(rawEMG);

  display.display();
}

// ============================================================
// NEOPIXEL
// TODOS LOS LEDS CAMBIAN JUNTOS ENTRE AZUL, BLANCO Y ROSA
// ============================================================
void updateTransGlitterAnimation() {
  if (!touchDetected) {
    for (int i = 0; i < NUM_PIXELS; i++) {
      uint32_t c = strip.getPixelColor(i);

      uint8_t r = (uint8_t)(c >> 16);
      uint8_t g = (uint8_t)(c >> 8);
      uint8_t b = (uint8_t)(c);

      r = fadeComponent(r, 12);
      g = fadeComponent(g, 12);
      b = fadeComponent(b, 12);

      strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
    return;
  }

  animationStep++;

  float x = animationStep * 0.015f;

  float wobble =
    0.15f * sin(x * 1.3f) +
    0.10f * sin(x * 2.1f) +
    0.06f * sin(x * 3.7f);

  float phase = (x + wobble);
  phase = phase - floor(phase);

  ColorRGB c = transCloudPalette(phase);

  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(c.r, c.g, c.b));
  }

  if (random(100) < 20) {
    int idx = random(NUM_PIXELS);

    uint32_t col = strip.getPixelColor(idx);

    uint8_t r = (uint8_t)(col >> 16);
    uint8_t g = (uint8_t)(col >> 8);
    uint8_t b = (uint8_t)(col);

    r = brightenComponent(r, 35);
    g = brightenComponent(g, 35);
    b = brightenComponent(b, 35);

    strip.setPixelColor(idx, strip.Color(r, g, b));
  }

  strip.show();
}

// ============================================================
// PALETA DE COLOR
// ============================================================
ColorRGB lerpColor(const ColorRGB& a, const ColorRGB& b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  ColorRGB out;
  out.r = (uint8_t)(a.r + (b.r - a.r) * t);
  out.g = (uint8_t)(a.g + (b.g - a.g) * t);
  out.b = (uint8_t)(a.b + (b.b - a.b) * t);
  return out;
}

ColorRGB transCloudPalette(float phase) {
  const ColorRGB blue  = { 70, 155, 255 };
  const ColorRGB white = { 245, 240, 250 };
  const ColorRGB pink  = { 255, 120, 195 };

  ColorRGB c;

  if (phase < 0.33f) {
    float t = phase / 0.33f;
    c = lerpColor(blue, white, t);
  } else if (phase < 0.66f) {
    float t = (phase - 0.33f) / 0.33f;
    c = lerpColor(white, pink, t);
  } else {
    float t = (phase - 0.66f) / 0.34f;
    c = lerpColor(pink, blue, t);
  }

  c.r = softenToPastelSubtle(c.r);
  c.g = softenToPastelSubtle(c.g);
  c.b = softenToPastelSubtle(c.b);

  return c;
}

// ============================================================
// UTILIDADES
// ============================================================
uint8_t brightenComponent(uint8_t value, uint8_t amount) {
  int result = value + amount;
  if (result > 255) result = 255;
  return (uint8_t)result;
}

uint8_t fadeComponent(uint8_t value, uint8_t amount) {
  if (value <= amount) return 0;
  return value - amount;
}

uint8_t softenToPastelSubtle(uint8_t value) {
  int softened = (value * 4 + 70) / 5;
  if (softened > 255) softened = 255;
  return (uint8_t)softened;
}