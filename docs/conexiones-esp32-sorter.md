# Conexiones — ESP32 #2 (sorter)

Bloque 1: servo de la puerta + botón. Sin WiFi — solo ESP-NOW. El diagrama
marca dónde está cada pin sobre la placa física real (no una caja
inventada), para que lo encuentres directo al armar.

![Conexiones del ESP32 sorter](img/esp32-sorter-conexiones.svg)

La señal del servo (GPIO13) y el botón (GPIO4) van directo al ESP32, pero
servo, botón, fuente externa y ESP32 comparten un único GND. Sin ese cable
de tierra común, el ESP32 se reinicia apenas el servo empieza a moverse —
ver la sección siguiente.

## Tabla de pines (Bloque 1)

| Señal | GPIO | Notas |
|---|---|---|
| Servo — Señal | 13 | PWM, `Servo.attach(13)` (librería ESP32Servo) |
| Servo — V+ | — | Fuente externa 5-12V, **no** el pin 5V del ESP32 |
| Servo — GND | — | Al GND común |
| Botón puerta | 4 | `INPUT_PULLUP`, antirrebote 40ms en firmware — no lleva resistencia externa |

Coincide con `PIN_SERVO_PUERTA` y `PIN_BOTON_PUERTA` en
[`firmware/esp32-sorter/src/main.cpp`](../firmware/esp32-sorter/src/main.cpp).

## Por qué la fuente externa (y por qué falla si no la pones)

El pin `5V` del ESP32 DevKit sale del regulador del propio USB, pensado para
unos pocos cientos de mA. Un servo bajo carga puede pedir picos de 500mA–1A
al arrancar a moverse. Si lo alimentás desde ahí, la caída de tensión
reinicia el ESP32 justo cuando el servo se mueve — el síntoma clásico es
"se reinicia solo al abrir la puerta". La fuente externa evita ese pico; el
**GND común es obligatorio** porque la señal PWM del ESP32 necesita una
referencia de tierra compartida con el servo para que el ángulo se
interprete bien.

## Pines reservados para el Bloque 2 — no los uses para otra cosa

Todavía no se arman, pero ya están apartados (marcados en gris punteado en
el diagrama de arriba, sobre su posición física real) para no chocar
cables mientras conectás el Bloque 1:

| Módulo | GPIO |
|---|---|
| TCS3472 (I2C) | SDA=21, SCL=22 |
| L298N (cinta) | ENA=25, IN1=26, IN2=27 |
| LEDs rojo (contador binario) | 16, 17, 5 |
| LEDs verde | 18, 19, 23 |
| LEDs azul | 32, 33, 12 |
| Botón inicio/stop cinta | 14 |

Entre el Bloque 1 y el Bloque 2 este ESP32 usa 17 de sus 30 pines físicos —
por eso vale la pena el diagrama de posición real en vez de una lista: al
armar a mano es fácil confundir un pin cercano en la placa por otro.
Referencia completa (sin resaltar nada):

![Pinout físico ESP32 DevKit](img/esp32-pinout-fisico.svg)

## Cómo el sorter encuentra el canal del gateway

Esta placa no se conecta a ningún WiFi, así que no tiene forma de saber de
antemano en qué canal quedó el gateway al conectarse al router. Lo resuelve
barriendo canales al arrancar:

![Barrido de canal ESP-NOW](img/esp32-sorter-barrido-canal.svg)

Peor caso: 13 canales × 400ms ≈ 5.2s hasta sincronizar. El gateway manda su heartbeat CMD_HELLO cada segundo apenas conecta al WiFi, así que en la práctica es casi siempre más rápido.

Implementado en `barrerCanalSiHaceFalta()` dentro de
[`firmware/esp32-sorter/src/main.cpp`](../firmware/esp32-sorter/src/main.cpp).
Limitación conocida: si el gateway se reconecta a un canal distinto después
de que el sorter ya sincronizó, no hay reintento automático — hay que
reiniciar el sorter. Aceptable para una demo de examen, no para producción.
