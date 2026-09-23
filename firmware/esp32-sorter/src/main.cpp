// ESP32 #2 — sorter. Bloque 1: servo (puerta) + botón, hablando solo por
// ESP-NOW (sin WiFi propio). Los campos de producción del protocolo
// (color, conteos, motor) se llenan recién en el Bloque 2.

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "protocol.h"

// --- Pines (Bloque 1) ---
constexpr int PIN_SERVO_PUERTA = 13;
constexpr int PIN_BOTON_PUERTA = 4;

// --- Puerta ---
constexpr int ANGULO_CERRADA = 0;
constexpr int ANGULO_ABIERTA = 90;
Servo servoPuerta;
bool puertaAbierta = false;

// --- Botón con antirrebote ---
constexpr unsigned long DEBOUNCE_MS = 40;
int estadoBotonAnterior = HIGH;
unsigned long ultimoCambioBoton = 0;

// --- Barrido de canal ESP-NOW ---
constexpr unsigned long MS_POR_CANAL = 400;
bool canalEncontrado = false;
int canalActual = 1;
unsigned long ultimoCambioCanal = 0;
volatile bool paqueteRecibido = false;

void aplicarPuerta(bool abrir) {
  puertaAbierta = abrir;
  servoPuerta.write(abrir ? ANGULO_ABIERTA : ANGULO_CERRADA);
}

void enviarEstado() {
  SorterMsg msg{};
  msg.msg_type = MSG_ESTADO;
  msg.color_id = COLOR_DESCONOCIDO;
  msg.motor_state = MOTOR_OFF;
  msg.door_open = puertaAbierta ? 1 : 0;
  msg.ts_ms = millis();
  esp_now_send(ESPNOW_BROADCAST_ADDR, reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
}

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  paqueteRecibido = true;  // basta con recibir algo para saber que el canal es correcto
  if (len != sizeof(CommandMsg)) return;

  CommandMsg cmd;
  memcpy(&cmd, data, sizeof(cmd));

  if (cmd.cmd == CMD_DOOR) {
    aplicarPuerta(cmd.arg == 1);
    enviarEstado();
  }
  // CMD_HELLO no requiere acción: solo sirve para que este equipo detecte el canal.
}

void iniciarEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_now_init();
  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, ESPNOW_BROADCAST_ADDR, 6);
  peer.channel = 0;  // usar el canal activo, sea cual sea
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

void barrerCanalSiHaceFalta() {
  if (canalEncontrado) return;

  if (paqueteRecibido) {
    canalEncontrado = true;
    Serial.printf("Canal ESP-NOW encontrado: %d\n", canalActual);
    return;
  }

  unsigned long ahora = millis();
  if (ahora - ultimoCambioCanal >= MS_POR_CANAL) {
    canalActual = (canalActual % 13) + 1;
    esp_wifi_set_channel(canalActual, WIFI_SECOND_CHAN_NONE);
    ultimoCambioCanal = ahora;
  }
}

void leerBoton() {
  int lectura = digitalRead(PIN_BOTON_PUERTA);
  if (lectura != estadoBotonAnterior) {
    ultimoCambioBoton = millis();
  }
  if ((millis() - ultimoCambioBoton) > DEBOUNCE_MS && lectura == LOW && estadoBotonAnterior == HIGH) {
    aplicarPuerta(!puertaAbierta);
    enviarEstado();
  }
  estadoBotonAnterior = lectura;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOTON_PUERTA, INPUT_PULLUP);

  servoPuerta.attach(PIN_SERVO_PUERTA);
  aplicarPuerta(false);

  iniciarEspNow();
  ultimoCambioCanal = millis();
}

void loop() {
  barrerCanalSiHaceFalta();
  if (canalEncontrado) {
    leerBoton();
  }
}
