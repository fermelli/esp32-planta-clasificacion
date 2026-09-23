# Documentación

Estado: **Bloque 1 (Acceso y puerta) y Bloque 2 (Producción y conteo)
completos** — backend, MQTT, firmware de los dos ESP32 y dashboard,
probados de punta a punta. Pendiente: calibrar el sensor de color con las
cajas reales, y flashear el hardware (falta SSID/contraseña del WiFi de la
demo e IP de la PC de escritorio en `firmware/esp32-gateway/include/config.h`).

- [`guia-armado.md`](./guia-armado.md) — **todos** los componentes de las
  dos placas juntos, en orden de armado, con checklist antes de encender.
- [`arquitectura.md`](./arquitectura.md) — el sistema completo: los 2 ESP32,
  Mosquitto, FastAPI, Postgres y el dashboard, y dónde corre cada uno.
- [`servidor.md`](./servidor.md) — estructura de `server/`, cómo correrlo,
  y la tabla completa de endpoints REST + WebSocket.
- [`dashboard.md`](./dashboard.md) — estructura de `dashboard/`, cómo
  correrlo, y cómo llegan los datos en vivo a cada página.
- [`conexiones-esp32-gateway.md`](./conexiones-esp32-gateway.md) — cableado
  del ESP32 #1: teclado 4x4 + LCD I2C, con tabla de pines.
- [`conexiones-esp32-sorter.md`](./conexiones-esp32-sorter.md) — cableado
  del ESP32 #2: servo + botón, cinta, sensor de color, LEDs binarios.
- [`protocolo.md`](./protocolo.md) — los structs de ESP-NOW y los topics de
  MQTT, con ejemplos reales de payload.

Los diagramas son archivos `.svg` sueltos en [`img/`](./img/) — se pueden
abrir directo (doble clic, o arrastrar al navegador) sin pasar por ningún
`.md`. Los de conexiones (`esp32-gateway-conexiones.svg`,
`esp32-sorter-conexiones.svg`) marcan la posición real de cada pin sobre el
layout físico de una DOIT ESP32 DEVKIT V1 de 30 pines — no son cajas
inventadas, es la misma plantilla verificada en
`esp32-potenciometro-led/docs/img/esp32-pinout-fisico.svg`. La referencia
completa sin resaltar nada:

![Pinout físico ESP32 DevKit](img/esp32-pinout-fisico.svg)

El plan completo del proyecto (los 4 bloques, la BBDD, qué se dejó fuera y
por qué) vive en `C:\Users\fermelli\.claude\plans\pero-tb-se-registra-peppy-raccoon.md`.
