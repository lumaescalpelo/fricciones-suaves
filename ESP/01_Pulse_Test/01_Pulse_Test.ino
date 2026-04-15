#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C

#define PULSE_PIN 34

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------- Configuración de tiempos ----------
const unsigned long pulseReadIntervalMs = 4;     // lectura rápida
const unsigned long displayUpdateMs     = 120;   // refresco OLED
const unsigned long serialUpdateMs      = 50;    // debug serial
const unsigned long beatTimeoutMs       = 2500;  // si no hay latido, BPM = 0

// ---------- Estado del pulso ----------
int rawPulse = 0;
int bpm = 0;

int thresholdHigh = 2300;   // ajustar según tu sensor
int thresholdLow  = 2100;   // histéresis para evitar rebotes

bool pulseStateHigh = false;
unsigned long lastBeatTime = 0;
unsigned long lastValidBeatInterval = 0;

// ---------- Timers ----------
unsigned long lastPulseReadTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastSerialTime = 0;

void setup() {
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) {
      // fallo crítico de display
    }
  }

  display.setRotation(2);  // 180 grados
  display.clearDisplay();
  display.display();

  analogReadResolution(12); // ESP32: 0-4095
}

void loop() {
  unsigned long now = millis();

  // 1) Leer sensor de pulso constantemente
  if (now - lastPulseReadTime >= pulseReadIntervalMs) {
    lastPulseReadTime = now;
    updatePulseSensor(now);
  }

  // 2) Si hace mucho que no hay latido, limpiar BPM
  if (lastBeatTime != 0 && (now - lastBeatTime > beatTimeoutMs)) {
    bpm = 0;
  }

  // 3) Actualizar pantalla aparte
  if (now - lastDisplayTime >= displayUpdateMs) {
    lastDisplayTime = now;
    updateDisplay();
  }

  // 4) Debug serial aparte
  if (now - lastSerialTime >= serialUpdateMs) {
    lastSerialTime = now;
    Serial.print("RAW: ");
    Serial.print(rawPulse);
    Serial.print("  BPM: ");
    Serial.println(bpm);
  }

  // Aquí después puedes agregar:
  // - lectura IMU
  // - touch
  // - humedad
  // - MQTT
  // todo con timers independientes
}

void updatePulseSensor(unsigned long now) {
  rawPulse = analogRead(PULSE_PIN);

  // Cruce por umbral alto = posible latido
  if (!pulseStateHigh && rawPulse >= thresholdHigh) {
    pulseStateHigh = true;

    if (lastBeatTime > 0) {
      unsigned long interval = now - lastBeatTime;

      // Filtrar latidos imposibles
      if (interval >= 300 && interval <= 2000) {
        lastValidBeatInterval = interval;
        bpm = 60000 / interval;
      }
    }

    lastBeatTime = now;
  }

  // Debe bajar del umbral bajo para habilitar el siguiente latido
  if (pulseStateHigh && rawPulse <= thresholdLow) {
    pulseStateHigh = false;
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
  display.println(bpm);

  display.setCursor(0, 20);
  display.print("RAW: ");
  display.println(rawPulse);

  display.display();
}