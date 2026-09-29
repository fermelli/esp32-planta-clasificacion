// ESP32 #1 — gateway. Bloque 1: teclado 4x4 + LCD I2C para el login, WiFi +
// MQTT hacia el servidor, y el puente ESP-NOW hacia el sorter (ESP32 #2).

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include "protocol.h"
#include "config.h"

// --- Teclado 4x4 ---
constexpr byte FILAS = 4, COLUMNAS = 4;
char TECLAS[FILAS][COLUMNAS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'},
};
byte pinesFilas[FILAS] = {13, 14, 27, 26};
byte pinesColumnas[COLUMNAS] = {25, 33, 32, 15};
Keypad teclado = Keypad(makeKeymap(TECLAS), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

LiquidCrystal_I2C lcd(0x27, 16, 2);  // direccion segun el escaner I2C (main.cpp escanea en setup());
                                     // este valor depende del modulo fisico soldado - reescanear si se cambia el modulo

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// --- Estado del login ---
String pinBuffer = "";
uint8_t intentoActual = 1;
bool esperandoRespuesta = false;
bool mostrandoMensaje = false;
unsigned long mensajeExpiraEn = 0;

// --- Heartbeat ESP-NOW ---
unsigned long ultimoHello = 0;
constexpr unsigned long HELLO_CADA_MS = 1000;

void lcdIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingrese PIN:");
  lcd.setCursor(0, 1);
  for (unsigned i = 0; i < pinBuffer.length(); i++) lcd.print('*');
}

// duracionMs == 0: el mensaje no vence solo, queda hasta la proxima tecla o
// el proximo mensaje.
void lcdMensaje(const String &linea1, const String &linea2, unsigned long duracionMs) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(linea1);
  lcd.setCursor(0, 1);
  lcd.print(linea2);
  mostrandoMensaje = true;
  mensajeExpiraEn = duracionMs ? millis() + duracionMs : 0;
}

// Igual que lcdMensaje, pero no interrumpe si el usuario está tecleando un
// PIN o esperando la respuesta del login — un evento de producción no debe
// robarle la siguiente tecla a alguien a mitad de su clave.
void lcdMensajeSiLibre(const String &linea1, const String &linea2, unsigned long duracionMs) {
  if (pinBuffer.length() > 0 || esperandoRespuesta) return;
  lcdMensaje(linea1, linea2, duracionMs);
}

const char *nombreColor(uint8_t colorId) {
  switch (colorId) {
    case COLOR_ROJO: return "rojo";
    case COLOR_VERDE: return "verde";
    case COLOR_AZUL: return "azul";
    default: return "desconocido";
  }
}

const char *nombreMotor(uint8_t estado) {
  switch (estado) {
    case MOTOR_LOW: return "low";
    case MOTOR_FULL: return "full";
    default: return "off";
  }
}

void enviarComandoAlSorter(uint8_t cmd, uint8_t arg) {
  CommandMsg msg{cmd, arg};
  esp_now_send(ESPNOW_BROADCAST_ADDR, reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
}

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  Serial.printf("ESP-NOW: paquete recibido, len=%d (esperado %d)\n", len, sizeof(SorterMsg));  // DIAGNOSTICO TEMPORAL
  if (len != sizeof(SorterMsg)) return;
  SorterMsg msg;
  memcpy(&msg, data, sizeof(msg));

  if (msg.msg_type == MSG_ESTADO) {
    JsonDocument doc;
    doc["puerta_abierta"] = msg.door_open == 1;
    doc["cinta_estado"] = nombreMotor(msg.motor_state);
    char buf[96];
    size_t n = serializeJson(doc, buf);
    // OJO: publish(topic, char*, size_t) no existe en PubSubClient. Sin el
    // cast, "n" (siempre > 0) se cuela como el 3er parametro real que SI
    // matchea: publish(topic, const char*, boolean retained) -> quedaba
    // publicando todo como retenido sin querer.
    mqtt.publish("planta/sorter/estado", reinterpret_cast<const uint8_t *>(buf), n);

  } else if (msg.msg_type == MSG_EVENTO_CAJA) {
    const char *color = nombreColor(msg.color_id);
    uint8_t conteo = msg.color_id == COLOR_ROJO ? msg.count_r
                    : msg.color_id == COLOR_VERDE ? msg.count_g
                    : msg.color_id == COLOR_AZUL ? msg.count_b : 0;

    JsonDocument doc;
    doc["color"] = color;
    doc["conteo"] = conteo;
    doc["lote_completo"] = msg.lote_completo == 1;
    doc["r"] = msg.r; doc["g"] = msg.g; doc["b"] = msg.b; doc["c"] = msg.c;
    char buf[160];
    size_t n = serializeJson(doc, buf);
    mqtt.publish("planta/sorter/evento", reinterpret_cast<const uint8_t *>(buf), n);

    // Color arriba y conteo (1-5) abajo; queda en el LCD hasta la proxima
    // caja o hasta que alguien toque el teclado.
    String titulo(color);
    titulo.setCharAt(0, toupper(titulo.charAt(0)));
    String detalle(conteo);
    if (msg.lote_completo) detalle += " Lote completo!";  // "5 Lote completo!" = 16 chars justos
    lcdMensajeSiLibre(titulo, detalle, 0);
  }
}

void iniciarEspNow() {
  esp_now_init();
  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, ESPNOW_BROADCAST_ADDR, 6);
  peer.channel = 0;  // el canal en el que ya quedó el WiFi STA
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

  String t(topic);

  if (t == "planta/login/resultado") {
    bool exito = doc["exito"];
    bool bloqueado = doc["bloqueado"];
    const char *nombre = doc["nombre"] | "";
    const char *motivo = doc["motivo"] | "";

    // PIN correcto con login por rostro activo en el servidor: todavia no hay
    // resultado, sigue esperando a que la camara verifique la cara.
    if (doc["requiere_rostro"] | false) {
      lcd.clear();
      lcd.print("Mire la camara");
      lcd.setCursor(0, 1);
      lcd.print(nombre);
      return;
    }

    if (exito) {
      lcdMensaje("Bienvenido", String(nombre), 2000);
    } else if (bloqueado) {
      lcdMensaje("Bloqueado", "2 intentos fallidos", 4000);
    } else if (strcmp(motivo, "rostro") == 0) {
      lcdMensaje("Rostro no valido", "Intento 2/2", 2000);
    } else {
      lcdMensaje("PIN incorrecto", "Intento 2/2", 2000);
    }
    pinBuffer = "";
    intentoActual = exito || bloqueado ? 1 : 2;
    esperandoRespuesta = false;

  } else if (t == "planta/cmd") {
    const char *cmd = doc["cmd"] | "";
    int arg = doc["arg"] | 0;
    if (strcmp(cmd, "puerta") == 0) {
      enviarComandoAlSorter(CMD_DOOR, arg);
    } else if (strcmp(cmd, "motor") == 0) {
      enviarComandoAlSorter(CMD_MOTOR_SPEED, arg);
    } else if (strcmp(cmd, "reset_counts") == 0) {
      enviarComandoAlSorter(CMD_RESET_COUNTS, 0);
    }
  }
}

void conectarWifi() {
  lcd.clear();
  lcd.print("Conectando WiFi");
  WiFi.mode(WIFI_STA);

  // --- DIAGNOSTICO TEMPORAL: lista redes visibles antes de intentar conectar ---
  Serial.println("\nBuscando redes WiFi visibles...");
  int n = WiFi.scanNetworks();
  bool ssidVisible = false;
  for (int i = 0; i < n; i++) {
    Serial.printf("  %2d) %-32s  RSSI:%4d  canal:%2d  %s\n", i + 1, WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "abierta" : "cifrada");
    if (WiFi.SSID(i) == WIFI_SSID) ssidVisible = true;
  }
  if (!ssidVisible) {
    Serial.printf("AVISO: \"%s\" no aparece en el escaneo. Puede ser red de 5GHz (invisible "
                  "para el ESP32) o el nombre no coincide exactamente.\n",
                  WIFI_SSID);
  }
  // --- fin diagnostico ---

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - inicio > 15000) {
      Serial.printf("\nNo conecto en 15s. WiFi.status()=%d "
                    "(0=idle 1=sin_ssid 3=conectado 4=fallo 5=perdida 6=desconectado)\n",
                    WiFi.status());
      inicio = millis();  // sigue intentando, pero re-imprime cada 15s
    }
  }
  Serial.printf("\nWiFi OK, canal %d, IP %s\n", WiFi.channel(), WiFi.localIP().toString().c_str());
}

void conectarMqtt() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  Serial.printf("Conectando a MQTT %s:%d...\n", MQTT_HOST, MQTT_PORT);
  while (!mqtt.connected()) {
    if (mqtt.connect("esp32-gateway")) {
      mqtt.subscribe("planta/login/resultado");
      mqtt.subscribe("planta/cmd");
      Serial.println("MQTT OK, conectado y suscrito");
    } else {
      // DIAGNOSTICO TEMPORAL: -4 timeout, -3 conexion perdida, -2 conexion
      // fallida (host/puerto malo), -1 desconectado, 1..5 rechazo del broker
      Serial.printf("MQTT fallo, state()=%d, reintento en 1s\n", mqtt.state());
      delay(1000);
    }
  }
}

void publicarIntentoLogin() {
  JsonDocument doc;
  doc["pin"] = pinBuffer;
  doc["intento"] = intentoActual;
  char buf[64];
  size_t n = serializeJson(doc, buf);
  mqtt.publish("planta/login/intento", buf, n);
  esperandoRespuesta = true;
  lcd.clear();
  lcd.print("Verificando...");
}

void manejarTeclado() {
  char tecla = teclado.getKey();
  if (tecla) Serial.printf("Tecla detectada: '%c'\n", tecla);  // DIAGNOSTICO TEMPORAL
  if (!tecla || esperandoRespuesta) return;

  if (mostrandoMensaje) {
    // Cualquier tecla saca del mensaje y ademas se procesa: como el conteo
    // de cajas queda fijo en pantalla, si se tragara la tecla se perderia
    // siempre el primer digito del PIN.
    mostrandoMensaje = false;
    lcdIdle();
  }

  if (tecla == '*') {
    pinBuffer = "";
    intentoActual = 1;
    lcdIdle();
  } else if (tecla == '#') {
    if (pinBuffer.length() > 0) publicarIntentoLogin();
  } else if (tecla == 'D') {
    if (pinBuffer.length() > 0) {
      pinBuffer.remove(pinBuffer.length() - 1);
      lcdIdle();
    }
  } else if (isDigit(tecla) && pinBuffer.length() < 8) {
    pinBuffer += tecla;
    lcdIdle();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);  // margen para que se abra el monitor y no perderse las primeras lineas

  // Antirrebote mas alto que el default (10ms): con un contacto flojo o
  // cableado ruidoso (p.ej. columna GPIO33, la que ya dio problemas antes)
  // una sola pulsacion puede generar varias transiciones seguidas.
  // Con 50ms seguia marcando 2 veces -> problema de conexion, no de rebote
  // mecanico normal; se sube igual pero no reemplaza revisar el cable.
  teclado.setDebounceTime(120);

  // --- DIAGNOSTICO TEMPORAL: escaner I2C, antes de tocar el LCD ---
  Serial.println("Escaneando bus I2C (SDA=21, SCL=22)...");
  Wire.begin();
  int encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Dispositivo I2C encontrado en 0x%02X\n", addr);
      encontrados++;
    }
  }
  if (encontrados == 0) {
    Serial.println("  Nada respondio. Revisar alimentacion (VIN, no 3V3) y cableado SDA/SCL.");
  }
  // --- fin diagnostico ---

  lcd.init();
  lcd.backlight();

  conectarWifi();
  iniciarEspNow();
  conectarMqtt();

  lcdIdle();
}

void loop() {
  if (!mqtt.connected()) conectarMqtt();
  mqtt.loop();

  if (mostrandoMensaje && mensajeExpiraEn != 0 && millis() > mensajeExpiraEn) {
    mostrandoMensaje = false;
    lcdIdle();
  }

  manejarTeclado();

  if (millis() - ultimoHello >= HELLO_CADA_MS) {
    enviarComandoAlSorter(CMD_HELLO, 0);
    ultimoHello = millis();
  }
}
