---
layout: home

hero:
  name: Planta de clasificación
  text: Documentación técnica
  tagline: COM520 — primer parcial. Bloque 1 y Bloque 2 completos — backend, MQTT, firmware de los dos ESP32 y dashboard, probado de punta a punta.
  actions:
    - theme: brand
      text: Arquitectura
      link: /arquitectura
    - theme: alt
      text: Protocolo
      link: /protocolo

features:
  - title: Arquitectura
    details: Los 2 ESP32, Mosquitto, FastAPI, Postgres y el dashboard — dónde corre cada pieza y por qué.
    link: /arquitectura
  - title: Servidor (FastAPI)
    details: Estructura de server/, cómo correrlo, y la tabla completa de endpoints REST + WebSocket.
    link: /servidor
  - title: Dashboard (Vue)
    details: Estructura de dashboard/, cómo correrlo, y cómo llegan los datos en vivo a cada página.
    link: /dashboard
  - title: ESP32 #1 — gateway
    details: Teclado 4x4 + LCD I2C, sobre el layout físico real de la placa.
    link: /conexiones-esp32-gateway
  - title: ESP32 #2 — sorter
    details: Servo, puerta, cinta, sensor de color y los 9 LEDs binarios.
    link: /conexiones-esp32-sorter
  - title: Protocolo
    details: Structs de ESP-NOW y topics MQTT, con payloads reales.
    link: /protocolo
---

## Pendiente

Calibrar `UMBRAL_PRESENCIA` del sensor de color con las cajas reales, y
flashear el hardware: SSID/contraseña del WiFi de la demo e IP de la PC de
escritorio, en `firmware/esp32-gateway/include/config.h`.

El plan completo del proyecto (los 4 bloques, la BBDD, qué se dejó fuera y
por qué) vive fuera de este repo, en el plan de la sesión de Claude Code.
