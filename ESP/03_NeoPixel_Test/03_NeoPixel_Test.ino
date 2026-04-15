#include <Wire.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

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
#define PULSE_PIN 34
#define TOUCH_PIN 4
#define NEOPIXEL_PIN 27
#define NUM_PIXELS 8

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

// ============================================================
// CONFIGURACION GENERAL
// ============================================================
const bool TOUCH_ACTIVE_HIGH = true;

// ============================================================
// INTERVALOS
// ============================================================
const unsigned long pulseReadIntervalMs = 4;
const unsigned long touchReadIntervalMs = 10;
const unsigned long displayUpdateMs     = 120;
const unsigned long ledUpdateMs         = 25;
const unsigned long beatTimeoutMs       = 2500;

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
// ESTADO TOUCH
// ============================================================
bool touchDetected = false;

// ============================================================
// ESTADO ANIMACION
// ============================================================
unsigned long animationStep = 0;

// ============================================================
// TIMERS
// ============================================================
unsigned long lastPulseReadTime = 0;
unsigned long lastTouchReadTime = 0;
unsigned long lastDisplayTime   = 0;
unsigned long lastLedTime       = 0;

// ============================================================
// PROTOTIPOS
// ============================================================
void updatePulseSensor(unsigned long now);
void updateTouchSensor();
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

  display.setRotation(2);  // 180 grados
  display.clearDisplay();
  display.display();

  analogReadResolution(12);   // ESP32 = 0 a 4095
  analogSetPinAttenuation(PULSE_PIN, ADC_11db);

  strip.begin();
  strip.show();
  strip.setBrightness(110);

  randomSeed(micros());
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  unsigned long now = millis();

  // Lectura de pulso
  if (now - lastPulseReadTime >= pulseReadIntervalMs) {
    lastPulseReadTime = now;
    updatePulseSensor(now);
  }

  // Lectura de touch
  if (now - lastTouchReadTime >= touchReadIntervalMs) {
    lastTouchReadTime = now;
    updateTouchSensor();
  }

  // Timeout BPM
  if (lastBeatTime != 0 && (now - lastBeatTime > beatTimeoutMs)) {
    bpm = 0;
  }

  // Actualizacion LEDs
  if (now - lastLedTime >= ledUpdateMs) {
    lastLedTime = now;
    updateTransGlitterAnimation();
  }

  // Actualizacion pantalla
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

      // Filtrar latidos imposibles
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
// OLED
// ============================================================
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("FRICCIONES SUAVES");

  display.setCursor(0, 10);
  display.print("BPM: ");
  display.print(bpm);

  // Asterisco si la lectura parece desconectada
  if (rawPulse < 50) {
    display.print("*");
  }

  display.setCursor(0, 20);
  display.print("TOUCH: ");
  display.println(touchDetected ? "O" : "X");

  display.display();
}

// ============================================================
// NEOPIXEL
// ============================================================
void updateTransGlitterAnimation() {

  // Fade out si no hay toque
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

  // 👇 UNA sola fase global
  float x = animationStep * 0.015f;

  // Variación suave (temporal, no espacial)
  float wobble =
    0.15f * sin(x * 1.3f) +
    0.10f * sin(x * 2.1f) +
    0.06f * sin(x * 3.7f);

  float phase = (x + wobble);
  phase = phase - floor(phase);  // 0..1

  // Obtener color único
  ColorRGB c = transCloudPalette(phase);

  // 👇 TODOS los LEDs iguales
  for (int i = 0; i < NUM_PIXELS; i++) {
    strip.setPixelColor(i, strip.Color(c.r, c.g, c.b));
  }

  // Glitter: algunos LEDs brillan un poco más momentáneamente
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
  // Colores definidos pero suaves
const ColorRGB blue  = { 70, 155, 255 };
const ColorRGB white = { 245, 240, 250 };
const ColorRGB pink  = { 255, 120, 195 };

  ColorRGB c;

  // Azul -> Blanco -> Rosa -> Azul
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

  // Pastel sutil para no lavar todo a blanco
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