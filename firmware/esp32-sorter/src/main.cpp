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
// lectura*Ant = última lectura cruda (para detectar el flanco y reiniciar el
// timer); estado*Estable = valor ya confirmado tras DEBOUNCE_MS, es contra
// esto que se dispara la acción una sola vez por pulsación.
constexpr unsigned long DEBOUNCE_MS = 40;
int lecturaBotonPuertaAnt = HIGH;
int estadoBotonPuertaEstable = HIGH;
unsigned long ultimoCambioBotonPuerta = 0;

// --- Botón cinta, antirrebote (toggle simple off/low; "full" es solo remoto) ---
int lecturaBotonCintaAnt = HIGH;
int estadoBotonCintaEstable = HIGH;
unsigned long ultimoCambioBotonCinta = 0;

// --- Motor / cinta ---
// Canal 4, no 0: la libreria ESP32Servo agarra el canal 0 para el servo de
// la puerta, y si el motor tambien pedia el canal 0 se "robaban" el canal
// entre si (el que se configura ultimo terminaba controlando el pin del otro).
constexpr int CANAL_PWM_MOTOR = 4;
constexpr int FREQ_PWM_MOTOR = 5000;
constexpr int RES_PWM_MOTOR = 8;  // 0-255
constexpr int DUTY_LOW = 235;     // ~92% -- ni 130 (~51%) ni 200 (~78%) alcanzaron
                                   // torque para vencer la friccion real de la cinta/motor
constexpr int DUTY_FULL = 255;
uint8_t motorState = MOTOR_OFF;

// --- Contadores binarios por color (0-5) ---
uint8_t countRojo = 0, countVerde = 0, countAzul = 0;

// --- Sensor de color ---
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
bool sensorOk = false;
bool cajaEnCurso = false;
// Con un solo umbral, el ruido del sensor hacia que "c" cruzara la linea
// muchas veces durante el paso de UNA sola caja (visto en vivo: 399 -> 451
// -> 1283 -> 408 -> 852... en milisegundos), y cada cruce disparaba un
// evento de color distinto -> LCD con conteo erratico, casi siempre rojo
// por el sesgo de las lecturas intermedias con poca luz. Dos umbrales con
// margen entre ellos, mas un cooldown, hacen que un solo paso cuente una
// sola vez. Calibrado contra el ambiente real (sin nada delante: c~250-310).
constexpr uint16_t UMBRAL_ENTRA = 450;      // cruzar esto hacia arriba: caja llegando
constexpr uint16_t UMBRAL_SALE = 300;       // hay que bajar de esto para rearmar
constexpr unsigned long MS_COOLDOWN_CAJA = 400;  // ignora nuevas cajas justo despues de una
unsigned long ultimaCajaMs = 0;

// --- Barrido de canal ESP-NOW ---
// El gateway manda CMD_HELLO cada 1s sin parar; si esta placa deja de
// recibir CUALQUIER paquete por mas de MS_SIN_SENAL ese tiempo, asume que
// el gateway cambio de canal (reinicio de una de las dos placas) y vuelve
// a barrer en vez de quedar sorda para siempre hasta un reset manual.
constexpr unsigned long MS_POR_CANAL = 400;
constexpr unsigned long MS_SIN_SENAL = 3000;
bool canalEncontrado = false;
int canalActual = 1;
unsigned long ultimoCambioCanal = 0;
volatile bool paqueteRecibido = false;
volatile unsigned long ultimoPaqueteMs = 0;

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

void enviarEventoCaja(uint8_t colorId, uint16_t r, uint16_t g, uint16_t b, uint16_t c,
                       uint8_t conteoDeEstaCaja, bool loteCompleto) {
  SorterMsg msg{};
  msg.msg_type = MSG_EVENTO_CAJA;
  msg.color_id = colorId;
  msg.r = r; msg.g = g; msg.b = b; msg.c = c;
  // El conteo de la caja que disparó este evento (1-5), no el contador ya
  // reseteado — si esta caja cerró el lote, count_* ya volvió a 0.
  msg.count_r = colorId == COLOR_ROJO ? conteoDeEstaCaja : countRojo;
  msg.count_g = colorId == COLOR_VERDE ? conteoDeEstaCaja : countVerde;
  msg.count_b = colorId == COLOR_AZUL ? conteoDeEstaCaja : countAzul;
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

// Devuelve el conteo de ESTA caja (1-5) para reportar al servidor, y deja en
// loteCompleto si fue la que cerró el lote. Los LEDs sí se resetean a 000 de
// inmediato al cerrar el lote (así lo especifica la pizarra); el valor
// reportado en el evento no, para que el dashboard vea "conteo 5, lote
// completo" en vez de "conteo 0, lote completo" (contradictorio).
uint8_t procesarConteo(uint8_t colorId, bool &loteCompleto) {
  loteCompleto = false;
  uint8_t *contador = nullptr;
  const int *pinesLed = nullptr;
  if (colorId == COLOR_ROJO) { contador = &countRojo; pinesLed = LED_ROJO; }
  else if (colorId == COLOR_VERDE) { contador = &countVerde; pinesLed = LED_VERDE; }
  else if (colorId == COLOR_AZUL) { contador = &countAzul; pinesLed = LED_AZUL; }
  else return 0;  // desconocido: no cuenta, no resetea

  (*contador)++;
  uint8_t conteoDeEstaCaja = *contador;
  if (*contador >= 5) {
    loteCompleto = true;
    *contador = 0;
  }
  mostrarContador(pinesLed, *contador);
  return conteoDeEstaCaja;
}

void leerSensorYClasificar() {
  if (!sensorOk) return;
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);

  // DIAGNOSTICO TEMPORAL: confirmar que ya no hay reintentos seguidos.
  static unsigned long ultimoPrintSensor = 0;
  if (millis() - ultimoPrintSensor >= 300) {
    ultimoPrintSensor = millis();
    Serial.printf("sensor: r=%u g=%u b=%u c=%u (entra=%u sale=%u) cajaEnCurso=%d\n", r, g, b, c,
                  UMBRAL_ENTRA, UMBRAL_SALE, cajaEnCurso);
  }

  if (!cajaEnCurso && c > UMBRAL_ENTRA && millis() - ultimaCajaMs >= MS_COOLDOWN_CAJA) {
    cajaEnCurso = true;
    ultimaCajaMs = millis();
    uint8_t colorId = clasificarColor(r, g, b, c);
    bool loteCompleto = false;
    uint8_t conteoDeEstaCaja = procesarConteo(colorId, loteCompleto);
    enviarEventoCaja(colorId, r, g, b, c, conteoDeEstaCaja, loteCompleto);
  } else if (cajaEnCurso && c <= UMBRAL_SALE) {
    cajaEnCurso = false;  // la caja ya pasó, listo para la próxima
  }
}

void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len) {
  // Filtra por tamaño y por comando valido antes de dar el canal por bueno:
  // en el aula hay otros equipos usando ESP-NOW broadcast tambien, y con
  // solo el tamaño (2 bytes, muy poco para distinguir) igual se enganchaba
  // con el canal de cualquiera de ellos en vez de esperar al gateway.
  if (len != sizeof(CommandMsg)) return;
  CommandMsg cmd;
  memcpy(&cmd, data, sizeof(cmd));
  if (cmd.cmd > CMD_RESET_COUNTS) return;  // no es un cmd del protocolo: no confiar en el canal

  paqueteRecibido = true;
  ultimoPaqueteMs = millis();

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
  if (canalEncontrado) {
    if (millis() - ultimoPaqueteMs < MS_SIN_SENAL) return;
    // Se corto la señal (el gateway probablemente reinicio en otro canal):
    // volver a barrer en vez de quedar sordo hasta un reset manual.
    Serial.println("Se perdio la señal del gateway, rebarriendo canal...");
    canalEncontrado = false;
    paqueteRecibido = false;
  }

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
  if (lectura != lecturaBotonPuertaAnt) ultimoCambioBotonPuerta = millis();

  if ((millis() - ultimoCambioBotonPuerta) > DEBOUNCE_MS && lectura != estadoBotonPuertaEstable) {
    estadoBotonPuertaEstable = lectura;
    if (estadoBotonPuertaEstable == LOW) {
      Serial.println("Boton puerta detectado, moviendo servo...");  // DIAGNOSTICO TEMPORAL
      aplicarPuerta(!puertaAbierta);
      enviarEstado();
    }
  }
  lecturaBotonPuertaAnt = lectura;
}

void leerBotonCinta() {
  int lectura = digitalRead(PIN_BOTON_CINTA);
  if (lectura != lecturaBotonCintaAnt) ultimoCambioBotonCinta = millis();

  if ((millis() - ultimoCambioBotonCinta) > DEBOUNCE_MS && lectura != estadoBotonCintaEstable) {
    estadoBotonCintaEstable = lectura;
    if (estadoBotonCintaEstable == LOW) {
      Serial.println("Boton cinta detectado, moviendo motor...");  // DIAGNOSTICO TEMPORAL
      // Cicla los 3 estados: off -> low -> full -> off
      uint8_t siguiente = motorState == MOTOR_OFF   ? MOTOR_LOW
                          : motorState == MOTOR_LOW  ? MOTOR_FULL
                                                      : MOTOR_OFF;
      aplicarMotor(siguiente);
      enviarEstado();
    }
  }
  lecturaBotonCintaAnt = lectura;
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

  // DIAGNOSTICO TEMPORAL: estado crudo de los pines de botones cada 500ms,
  // para ver si se mueven del todo aunque sea un instante al presionar.
  static unsigned long ultimoPrint = 0;
  if (millis() - ultimoPrint >= 500) {
    ultimoPrint = millis();
    Serial.printf("raw: puerta(GPIO4)=%d cinta(GPIO14)=%d\n", digitalRead(PIN_BOTON_PUERTA),
                  digitalRead(PIN_BOTON_CINTA));
  }
}
