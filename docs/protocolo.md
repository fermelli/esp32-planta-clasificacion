# Protocolo: ESP-NOW y MQTT

Los mensajes que efectivamente cruzan cada hop, con su forma real — no solo
los nombres de los topics.

![Traducción de protocolo en el gateway](img/protocolo-traduccion.svg)

El gateway es el traductor: structs binarios de tamaño fijo hacia el sorter, JSON hacia el servidor. Ningún otro nodo habla los dos idiomas.

## ESP-NOW — `firmware/shared/protocol.h`

### `SorterMsg` (#2 → #1)

```c
struct __attribute__((packed)) SorterMsg {
  uint8_t msg_type;      // 1=evento_caja 2=estado 3=alerta
  uint8_t color_id;      // 0=rojo 1=verde 2=azul 3=desconocido
  uint16_t r, g, b, c;   // lectura cruda TCS3472 — Bloque 2
  uint8_t count_r, count_g, count_b;  // Bloque 2
  uint8_t motor_state;   // 0=off 1=low 2=full — Bloque 2
  uint8_t door_open;     // Bloque 1
  uint32_t ts_ms;
};
```

En el Bloque 1 solo se usan `msg_type=MSG_ESTADO` y `door_open`; el resto de
los campos de producción viajan en 0 hasta que el Bloque 2 los llene — el
struct se definió completo desde el principio para no tener que tocarlo ni
recompilar las dos placas a mitad de camino.

### `CommandMsg` (#1 → #2)

```c
struct __attribute__((packed)) CommandMsg {
  uint8_t cmd;   // 0=hello 1=start 2=stop 3=motor_speed 4=door 5=reset_counts
  uint8_t arg;
};
```

`CMD_HELLO` (0) no es un comando real: es el heartbeat que manda el gateway
cada segundo y que el sorter usa para el barrido de canal (ver
[`conexiones-esp32-sorter.md`](./conexiones-esp32-sorter.md)).

## MQTT — topics del Bloque 1

| Topic | Dirección | Payload | Quién lo usa |
|---|---|---|---|
| `planta/login/intento` | ESP32 #1 → servidor | `{"pin": "1234", "intento": 1}` | Gateway publica al presionar `#` en el teclado |
| `planta/login/resultado` | servidor → ESP32 #1 | `{"exito": true, "nombre": "operador1", "bloqueado": false, "intento": 1}` | Servidor responde tras validar contra `usuarios` |
| `planta/sorter/estado` | ESP32 #1 → servidor | `{"puerta_abierta": true}` | Gateway reenvía lo que le llega por ESP-NOW; el servidor solo lo relay-ea por WebSocket, no lo persiste |
| `planta/cmd` | servidor → ESP32 #1 | `{"cmd": "puerta", "arg": 1}` | Dashboard pide abrir/cerrar la puerta (`POST /api/comandos`) |

`nombre` viene `null` cuando el PIN no corresponde a nadie; `bloqueado` es
`true` cuando el intento fallido es el número 2 — ahí el servidor también
inserta una fila en `alertas`.

Probado de punta a punta con `mosquitto_pub`/`mosquitto_sub` antes de tener
hardware — ver el historial de commits de `server/app/mqtt_client.py`.

## Ver también

- [`arquitectura.md`](./arquitectura.md) — dónde corre cada pieza
- [`conexiones-esp32-gateway.md`](./conexiones-esp32-gateway.md)
- [`conexiones-esp32-sorter.md`](./conexiones-esp32-sorter.md)
