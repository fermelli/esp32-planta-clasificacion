// Sketch minimo para probar el LCD I2C solo (sin WiFi/MQTT/teclado).
// Objetivo: aislar el LCD del resto del gateway para depurar mas rapido.

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Cambiar la direccion si el escaneo de abajo encuentra otra.
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("Escaneando I2C (SDA=21, SCL=22)...");
  Wire.begin();
  int encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Encontrado en 0x%02X\n", addr);
      encontrados++;
    }
  }
  if (encontrados == 0) Serial.println("  Nada respondio.");

  lcd.init();
  lcd.backlight();

  // Linea 1 fija: sirve para ver si el contraste deja leer letras y numeros.
  lcd.setCursor(0, 0);
  lcd.print("HOLA 0123456789");

  Serial.println("Setup listo. Deberia verse 'HOLA 0123456789' en la linea 1.");
}

void loop() {
  // Linea 2: contador que cambia cada segundo, para confirmar que el LCD
  // se sigue actualizando de verdad (no es una imagen congelada del arranque).
  static unsigned long ultimo = 0;
  static int contador = 0;
  if (millis() - ultimo >= 1000) {
    ultimo = millis();
    lcd.setCursor(0, 1);
    lcd.printf("Contando: %4d", contador);
    Serial.printf("contador=%d\n", contador);
    contador++;
  }
}
