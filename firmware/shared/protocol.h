#pragma once
#include <cstdint>

// Structs compartidos por ESP-NOW entre esp32-gateway (ESP32 #1) y
// esp32-sorter (ESP32 #2). Se definen completos desde el Bloque 1 para no
// tener que tocarlos en el Bloque 2: los campos de producción (conteos,
// motor, color) van en 0 hasta que ese bloque los llene de verdad.

enum SorterMsgType : uint8_t {
  MSG_EVENTO_CAJA = 1,
  MSG_ESTADO = 2,
  MSG_ALERTA = 3,
};

enum ComandoCmd : uint8_t {
  CMD_HELLO = 0,        // heartbeat del gateway, usado también para el barrido de canal
  CMD_START = 1,
  CMD_STOP = 2,
  CMD_MOTOR_SPEED = 3,
  CMD_DOOR = 4,          // arg: 1 = abrir, 0 = cerrar
  CMD_RESET_COUNTS = 5,
};

enum MotorState : uint8_t {
  MOTOR_OFF = 0,
  MOTOR_LOW = 1,
  MOTOR_FULL = 2,
};

enum ColorId : uint8_t {
  COLOR_ROJO = 0,
  COLOR_VERDE = 1,
  COLOR_AZUL = 2,
  COLOR_DESCONOCIDO = 3,
};

struct __attribute__((packed)) SorterMsg {   // #2 -> #1
  uint8_t msg_type;
  uint8_t color_id;
  uint16_t r, g, b, c;
  uint8_t count_r, count_g, count_b;
  uint8_t motor_state;
  uint8_t door_open;
  uint32_t ts_ms;
};

struct __attribute__((packed)) CommandMsg {  // #1 -> #2
  uint8_t cmd;
  uint8_t arg;
};

// Dirección de broadcast ESP-NOW: ambas placas se registran como peer de esta
// dirección con channel=0 ("usar el canal activo"), así no hay que conocer ni
// fijar la MAC de la otra placa de antemano.
// 'const' a nivel de archivo tiene enlace interno en C++, así que incluir esto
// en dos .cpp no genera símbolos duplicados (no hace falta 'inline'/C++17).
const uint8_t ESPNOW_BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
