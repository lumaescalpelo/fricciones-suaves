#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Tamaño de pantalla
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// I2C
#define OLED_SDA 21
#define OLED_SCL 22

// Dirección típica
#define OLED_ADDR 0x3C

// Crear objeto display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  // Iniciar I2C
  Wire.begin(OLED_SDA, OLED_SCL);

  // Inicializar display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while(true); // Si falla, se queda aquí
  }

  display.clearDisplay();

  // Configuración de texto
  display.setTextSize(1);        // 👈 Tamaño más pequeño posible
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);       // 👈 Arriba izquierda

  // Texto
  display.println("FRICCIONES SUAVES");

  display.display();
}

void loop() {
  // Nada aquí, esto es solo prueba estática
}