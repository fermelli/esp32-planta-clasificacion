---
layout: home

hero:
  name: Planta de clasificación
  text: Documentación técnica
  tagline: COM520 — primer parcial. Bloque 1 (Acceso y puerta) completo — backend, MQTT y firmware de los dos ESP32, probado de punta a punta.
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
  - title: ESP32 #1 — gateway
    details: Teclado 4x4 + LCD I2C, sobre el layout físico real de la placa.
    link: /conexiones-esp32-gateway
  - title: ESP32 #2 — sorter
    details: Servo + botón (Bloque 1) y los pines ya reservados para el Bloque 2.
    link: /conexiones-esp32-sorter
  - title: Protocolo
    details: Structs de ESP-NOW y topics MQTT del Bloque 1, con payloads reales.
    link: /protocolo
---

## Pendiente

Falta flashear el hardware real: SSID/contraseña del WiFi de la demo e IP
de la PC de escritorio, en `firmware/esp32-gateway/include/config.h`.

El plan completo del proyecto (los 4 bloques, la BBDD, qué se dejó fuera y
por qué) vive fuera de este repo, en el plan de la sesión de Claude Code.
