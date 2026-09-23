# Planta de clasificación — COM520 primer parcial

Sistema de clasificación de cajas por color: 2 ESP32 (login por teclado +
LCD, y servo/cinta/sensor/LEDs), un servidor FastAPI con Postgres y MQTT,
y un dashboard en Vue con datos en vivo por WebSocket.

**Repo:** [github.com/fermelli/esp32-planta-clasificacion](https://github.com/fermelli/esp32-planta-clasificacion)

```bash
git clone https://github.com/fermelli/esp32-planta-clasificacion.git
```

**Documentación completa:** [`docs/`](./docs/) — o `npm run docs:dev` para
navegarla con VitePress (sidebar, búsqueda). Empezá por
[`docs/guia-armado.md`](./docs/guia-armado.md) si vas a cablear el
hardware, o por [`docs/README.md`](./docs/README.md) como índice general.

## Estructura

```
firmware/   # ESP32 #1 (gateway) y ESP32 #2 (sorter), PlatformIO
server/     # FastAPI + Postgres + Mosquitto (Docker)
dashboard/  # Vue 3 + TypeScript + Vite
docs/       # toda la documentación técnica, con diagramas
```

## Arrancar todo

```bash
# 1. Backend
cd server
cp .env.example .env          # editar JWT_SECRET y el secreto de siembra de cada usuario
docker compose up -d          # Postgres + Mosquitto
python -m venv .venv && .venv/Scripts/pip install -r requirements.txt
.venv/Scripts/python seed.py
.venv/Scripts/python -m uvicorn app.main:app --host 0.0.0.0 --port 8000

# 2. Dashboard (otra terminal)
cd dashboard
cp .env.example .env.local    # apuntar a la IP de la PC donde corre el servidor
npm install
npm run dev

# 3. Firmware: abrir firmware/esp32-gateway o firmware/esp32-sorter en
#    PlatformIO y flashear cada ESP32 — ver docs/guia-armado.md
```

Detalle completo de cada pieza (endpoints, variables de entorno, por qué
está armado así) en [`docs/servidor.md`](./docs/servidor.md) y
[`docs/dashboard.md`](./docs/dashboard.md).
