// ESP32 #2 — sorter. Bloque 1: servo (puerta) + botón puerta.
// Bloque 2: cinta (motor DC + L298N), sensor de color TCS3472, 9 LEDs como
// 3 contadores binarios (0-5) y botón inicio/stop de la cinta.
// Habla solo por ESP-NOW, sin WiFi propio.

#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include "protocol.h"

// --- Pines Bloque 1 ---
constexpr int PIN_SERVO_PUERTA = 13;
constexpr int PIN_BOTON_PUERTA = 4;

// --- Pines Bloque 2 ---
constexpr int PIN_BOTON_CINTA = 14;
constexpr int PIN_MOTOR_ENA = 25;
constexpr int PIN_MOTOR_IN1 = 26;
constexpr int PIN_MOTOR_IN2 = 27;
constexpr int LED_ROJO[3] = {16, 17, 5};
constexpr int LED_VERDE[3] = {18, 19, 23};
constexpr int LED_AZUL[3] = {32, 33, 12};

// --- Puerta ---
constexpr int ANGULO_CERRADA = 0;
constexpr int ANGULO_ABIERTA = 90;
Servo servoPuerta;
bool puertaAbierta = false;

// --- Botón puerta, antirrebote ---
constexpr unsigned long DEBOUNCE_MS = 40;
int estadoBotonPuertaAnt = HIGH;
unsigned long ultimoCambioBotonPuerta = 0;

// --- Botón cinta, antirrebote (toggle simple off/low; "full" es solo remoto) ---
int estadoBotonCintaAnt = HIGH;
unsigned long ultimoCambioBotonCinta = 0;

// --- Motor / cinta ---
constexpr int CANAL_PWM_MOTOR = 0;
constexpr int FREQ_PWM_MOTOR = 5000;
constexpr int RES_PWM_MOTOR = 8;  // 0-255
constexpr int DUTY_LOW = 130;     // ~51%
constexpr int DUTY_FULL = 255;
uint8_t motorState = MOTOR_OFF;

// --- Contadores binarios por color (0-5) ---
uint8_t countRojo = 0, countVerde = 0, countAzul = 0;

// --- Sensor de color ---
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
bool sensorOk = false;
bool cajaEnCurso = false;
// Calibrar con las cajas reales: el clear (c) sube cuando algo tapa el sensor.
// Ajustar UMBRAL_PRESENCIA al valor de "sin nada delante" + margen.
constexpr uint16_t UMBRAL_PRESENCIA = 400;

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

void aplicarMotor(uint8_t estado) {
  motorState = estado;
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  int duty = 0;
  if (estado == MOTOR_LOW) duty = DUTY_LOW;
  else if (estado == MOTOR_FULL) duty = DUTY_FULL;
  ledcWrite(CANAL_PWM_MOTOR, duty);
}

void mostrarContador(const int pines[3], uint8_t valor) {
  digitalWrite(pines[0], (valor >> 2) & 1);
  digitalWrite(pines[1], (valor >> 1) & 1);
  digitalWrite(pines[2], valor & 1);
}

void enviarEstado() {
  SorterMsg msg{};
  msg.msg_type = MSG_ESTADO;
  msg.color_id = COLOR_DESCONOCIDO;
  msg.motor_state = motorState;
  msg.door_open = puertaAbierta ? 1 : 0;
  msg.ts_ms = millis();
  esp_now_send(ESPNOW_BROADCAST_ADDR, reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
}

void enviarEventoCaja(uint8_t colorId, uint16_t r, uint16_t g, uint16_t b, uint16_t c, bool loteCompleto) {
  SorterMsg msg{};
  msg.msg_type = MSG_EVENTO_CAJA;
  msg.color_id = colorId;
  msg.r = r; msg.g = g; msg.b = b; msg.c = c;
  msg.count_r = countRojo; msg.count_g = countVerde; msg.count_b = countAzul;
  msg.lote_completo = loteCompleto ? 1 : 0;
  msg.motor_state = motorState;
  msg.door_open = puertaAbierta ? 1 : 0;
  msg.ts_ms = millis();
  esp_now_send(ESPNOW_BROADCAST_ADDR, reinterpret_cast<uint8_t *>(&msg), sizeof(msg));
}

uint8_t clasificarColor(uint16_t r, uint16_t g, uint16_t b, uint16_t c) {
  if (c == 0) return COLOR_DESCONOCIDO;
  // Normalizado por 'clear': robusto a que la caja esté más o menos cerca,
  // o a que cambie el brillo ambiente del aula.
  float rn = (float)r / c, gn = (float)g / c, bn = (float)b / c;
  if (rn > gn && rn > bn) return COLOR_ROJO;
  if (gn > rn && gn > bn) return COLOR_VERDE;
  if (bn > rn && bn > gn) return COLOR_AZUL;
  return COLOR_DESCONOCIDO;
}

bool procesarConteo(uint8_t colorId) {
  uint8_t *contador = nullptr;
  const int *pinesLed = nullptr;
  if (colorId == COLOR_ROJO) { contador = &countRojo; pinesLed = LED_ROJO; }
  else if (colorId == COLOR_VERDE) { contador = &countVerde; pinesLed = LED_VERDE; }
  else if (colorId == COLOR_AZUL) { contador = &countAzul; pinesLed = LED_AZUL; }
  else return false;  // desconocido: no cuenta, no resetea

  (*contador)++;
  bool loteCompleto = false;
  if (*contador >= 5) {
    loteCompleto = true;
    *contador = 0;
  }
  mostrarContador(pinesLed, *contador);
  return loteCompleto;
}

void leerSensorYClasificar() {
  if (!sensorOk) return;
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  if (!cajaEnCurso && c > UMBRAL_PRESENCIA) {
    cajaEnCurso = true;
    uint8_t colorId = clasificarColor(r, g, b, c);
    bool loteCompleto = procesarConteo(colorId);
    enviarEventoCaja(colorId, r, g, b, c, loteCompleto);
  } else if (cajaEnCurso && c <= UMBRAL_PRESENCIA) {
    cajaEnCurso = false;  // la caja ya pasó, listo para la próxima
  }
}

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  paqueteRecibido = true;  // basta con recibir algo para saber que el canal es correcto
  if (len != sizeof(CommandMsg)) return;

  CommandMsg cmd;
  memcpy(&cmd, data, sizeof(cmd));

  if (cmd.cmd == CMD_DOOR) {
    aplicarPuerta(cmd.arg == 1);
    enviarEstado();
  } else if (cmd.cmd == CMD_START) {
    aplicarMotor(MOTOR_LOW);
    enviarEstado();
  } else if (cmd.cmd == CMD_STOP) {
    aplicarMotor(MOTOR_OFF);
    enviarEstado();
  } else if (cmd.cmd == CMD_MOTOR_SPEED) {
    aplicarMotor(cmd.arg);
    enviarEstado();
  } else if (cmd.cmd == CMD_RESET_COUNTS) {
    countRojo = countVerde = countAzul = 0;
    mostrarContador(LED_ROJO, 0);
    mostrarContador(LED_VERDE, 0);
    mostrarContador(LED_AZUL, 0);
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

void leerBotonPuerta() {
  int lectura = digitalRead(PIN_BOTON_PUERTA);
  if (lectura != estadoBotonPuertaAnt) ultimoCambioBotonPuerta = millis();
  if ((millis() - ultimoCambioBotonPuerta) > DEBOUNCE_MS && lectura == LOW && estadoBotonPuertaAnt == HIGH) {
    aplicarPuerta(!puertaAbierta);
    enviarEstado();
  }
  estadoBotonPuertaAnt = lectura;
}

void leerBotonCinta() {
  int lectura = digitalRead(PIN_BOTON_CINTA);
  if (lectura != estadoBotonCintaAnt) ultimoCambioBotonCinta = millis();
  if ((millis() - ultimoCambioBotonCinta) > DEBOUNCE_MS && lectura == LOW && estadoBotonCintaAnt == HIGH) {
    aplicarMotor(motorState == MOTOR_OFF ? MOTOR_LOW : MOTOR_OFF);
    enviarEstado();
  }
  estadoBotonCintaAnt = lectura;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_BOTON_PUERTA, INPUT_PULLUP);
  pinMode(PIN_BOTON_CINTA, INPUT_PULLUP);
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  for (int p : LED_ROJO) pinMode(p, OUTPUT);
  for (int p : LED_VERDE) pinMode(p, OUTPUT);
  for (int p : LED_AZUL) pinMode(p, OUTPUT);

  ledcSetup(CANAL_PWM_MOTOR, FREQ_PWM_MOTOR, RES_PWM_MOTOR);
  ledcAttachPin(PIN_MOTOR_ENA, CANAL_PWM_MOTOR);
  aplicarMotor(MOTOR_OFF);

  servoPuerta.attach(PIN_SERVO_PUERTA);
  aplicarPuerta(false);

  sensorOk = tcs.begin();
  if (!sensorOk) Serial.println("TCS3472 no responde — revisar cableado I2C (SDA=21, SCL=22)");

  iniciarEspNow();
  ultimoCambioCanal = millis();
}

void loop() {
#ifdef MODO_STANDALONE
  // Sin gateway al lado no hay con quién sincronizar canal — se salta la
  // espera para poder probar botones/motor/sensor/LEDs de esta placa sola.
  leerBotonPuerta();
  leerBotonCinta();
  leerSensorYClasificar();
#else
  barrerCanalSiHaceFalta();
  if (canalEncontrado) {
    leerBotonPuerta();
    leerBotonCinta();
    leerSensorYClasificar();
  }
#endif
}
