# Planta de clasificación — COM520 primer parcial

Sistema de clasificación de cajas por color: 2 ESP32 (login por teclado +
LCD, y servo/cinta/sensor/LEDs), un servidor FastAPI con Postgres y MQTT,
un dashboard en Vue con datos en vivo por WebSocket, y una ESP32-S3-CAM con
IA en dos versiones (login por rostro y clasificación de color).

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
firmware/   # ESP32 #1 (gateway), #2 (sorter) y ESP32-S3-CAM (esp32-cam-rostro / esp32-cam-color), PlatformIO
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

## Cámara con IA (opcional, apagada por defecto)

Dos versiones que se prenden con variables de `server/.env` (los dos flags
vienen en `false`: sin ellos todo funciona como antes). La IA corre en la
laptop con OpenCV y scikit-learn, sin GPU.

| Versión | Variable            | Qué hace                                                    | Doc                                                |
| ------- | ------------------- | ----------------------------------------------------------- | -------------------------------------------------- |
| A       | `LOGIN_ROSTRO=true` | El login del teclado pide PIN **y** rostro                  | [`docs/camara-rostro.md`](./docs/camara-rostro.md) |
| B       | `CAMARA_COLOR=true` | Foto de cada caja, clasificada por IA en paralelo al sensor | [`docs/camara-color.md`](./docs/camara-color.md)   |

Detalle completo de cada pieza (endpoints, variables de entorno, por qué
está armado así) en [`docs/servidor.md`](./docs/servidor.md) y
[`docs/dashboard.md`](./docs/dashboard.md).

## Formato y lint

`npm install` en la raíz deja activo un pre-commit (husky + lint-staged) que
corre Prettier y ESLint sobre los archivos staged; si algo queda sin poder
arreglarse solo, el commit se aborta.
