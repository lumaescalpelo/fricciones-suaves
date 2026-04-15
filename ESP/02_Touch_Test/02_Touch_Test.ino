#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

#define PULSE_PIN 34
#define TOUCH_PIN 4

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Configuración touch ----------
const bool TOUCH_ACTIVE_HIGH = true;  
// true  = el pin se pone HIGH al tocar
// false = el pin se pone LOW al tocar

// ---------- Configuración de tiempos ----------
const unsigned long pulseReadIntervalMs = 4;
const unsigned long touchReadIntervalMs = 10;
const unsigned long displayUpdateMs     = 120;
const unsigned long serialUpdateMs      = 50;
const unsigned long beatTimeoutMs       = 2500;

// ---------- Estado del pulso ----------
int rawPulse = 0;
int bpm = 0;

int thresholdHigh = 2300;
int thresholdLow  = 2100;

bool pulseStateHigh = false;
unsigned long lastBeatTime = 0;
unsigned long lastValidBeatInterval = 0;

// ---------- Estado touch ----------
bool touchDetected = false;

// ---------- Timers ----------
unsigned long lastPulseReadTime = 0;
unsigned long lastTouchReadTime = 0;
unsigned long lastDisplayTime   = 0;
unsigned long lastSerialTime    = 0;

void setup() {
  Serial.begin(115200);

  pinMode(TOUCH_PIN, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) {
      // fallo crítico de display
    }
  }

  display.setRotation(2);
  display.clearDisplay();
  display.display();

  analogReadResolution(12);
}

void loop() {
  unsigned long now = millis();

  // 1) Leer pulso constantemente
  if (now - lastPulseReadTime >= pulseReadIntervalMs) {
    lastPulseReadTime = now;
    updatePulseSensor(now);
  }

  // 2) Leer touch
  if (now - lastTouchReadTime >= touchReadIntervalMs) {
    lastTouchReadTime = now;
    updateTouchSensor();
  }

  // 3) Si hace mucho que no hay latido, limpiar BPM
  if (lastBeatTime != 0 && (now - lastBeatTime > beatTimeoutMs)) {
    bpm = 0;
  }

  // 4) Actualizar pantalla aparte
  if (now - lastDisplayTime >= displayUpdateMs) {
    lastDisplayTime = now;
    updateDisplay();
  }

  // 5) Debug serial aparte
  if (now - lastSerialTime >= serialUpdateMs) {
    lastSerialTime = now;
    Serial.print("RAW: ");
    Serial.print(rawPulse);
    Serial.print("  BPM: ");
    Serial.print(bpm);
    Serial.print("  TOUCH: ");
    Serial.println(touchDetected ? "O" : "X");
  }

  // Después aquí puedes agregar más sensores con su propio intervalo
}

void updatePulseSensor(unsigned long now) {
  rawPulse = analogRead(PULSE_PIN);

  if (!pulseStateHigh && rawPulse >= thresholdHigh) {
    pulseStateHigh = true;

    if (lastBeatTime > 0) {
      unsigned long interval = now - lastBeatTime;

      if (interval >= 300 && interval <= 2000) {
        lastValidBeatInterval = interval;
        bpm = 60000 / interval;
      }
    }

    lastBeatTime = now;
  }

  if (pulseStateHigh && rawPulse <= thresholdLow) {
    pulseStateHigh = false;
  }
}

void updateTouchSensor() {
  int rawTouch = digitalRead(TOUCH_PIN);

  if (TOUCH_ACTIVE_HIGH) {
    touchDetected = (rawTouch == HIGH);
  } else {
    touchDetected = (rawTouch == LOW);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("FRICCIONES SUAVES");

  display.setCursor(0, 10);
  display.print("BPM: ");
  display.print(bpm);

  // 👇 Asterisco si no hay señal
  if (rawPulse == 0) {
    display.print("*");
  }

  display.setCursor(0, 20);
  display.print("TOUCH: ");
  display.println(touchDetected ? "O" : "X");

  display.display();
}