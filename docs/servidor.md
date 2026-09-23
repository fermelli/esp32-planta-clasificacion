# Servidor (FastAPI)

`server/` es el backend: Postgres + Mosquitto en Docker, y FastAPI corriendo
aparte (fuera de Docker, directo con `uvicorn`) porque necesita reiniciarse
seguido durante el desarrollo.

## Estructura

```
server/
├── app/
│   ├── main.py            # arma la app, lifespan (BBDD + MQTT), /ws, /api/salud
│   ├── config.py          # variables de entorno (pydantic-settings)
│   ├── db.py               # pool de asyncpg
│   ├── security.py         # hashing (bcrypt) y JWT
│   ├── deps.py              # get_current_user (protege los endpoints)
│   ├── mqtt_client.py       # cliente MQTT: valida, persiste, retransmite por WS
│   ├── ws.py                 # ConnectionManager del WebSocket
│   ├── schemas.py             # modelos Pydantic de entrada/salida
│   └── routers/
│       ├── auth.py             # POST /api/auth/login
│       ├── intentos.py          # GET /api/intentos, /api/alertas
│       ├── comandos.py           # POST /api/comandos
│       └── produccion.py          # GET /api/eventos, /api/conteos, /api/produccion/historico
├── schema.sql               # esquema del Bloque 1 (se auto-aplica al crear el contenedor)
├── migrations/               # esquema de bloques siguientes, se monta aparte
├── seed.py                    # siembra los 2 usuarios (idempotente)
├── docker-compose.yml           # Postgres + Mosquitto
└── .env.example
```

## Cómo correrlo

```bash
cd server
cp .env.example .env          # editar JWT_SECRET y el secreto de siembra de cada usuario
docker compose up -d          # Postgres + Mosquitto
python -m venv .venv && .venv/Scripts/pip install -r requirements.txt
.venv/Scripts/python seed.py  # siembra los 2 usuarios
.venv/Scripts/python -m uvicorn app.main:app --host 0.0.0.0 --port 8000
```

`--host 0.0.0.0` importa: es lo que permite que el ESP32 (u otra máquina de
la red) le llegue por la IP de esta PC, no solo `localhost`.

## Endpoints REST

Todos menos `/api/salud` y `/api/auth/login` requieren `Authorization: Bearer <token>`.

| Método | Ruta | Qué hace |
|---|---|---|
| `POST` | `/api/auth/login` | `{nombre, password}` → JWT. Registra el intento (origen `web`) |
| `GET` | `/api/intentos?limite=50` | Historial de login (`keypad` + `web`) |
| `GET` | `/api/alertas?limite=50` | Alertas (`login_bloqueado`, `lote_completo`) |
| `POST` | `/api/comandos` | `{cmd, arg}` → publica en `planta/cmd`. `cmd`: `puerta`, `motor`, `reset_counts` |
| `GET` | `/api/eventos?limite=50` | Últimas cajas, con lectura cruda del sensor |
| `GET` | `/api/conteos` | Conteo actual + total histórico + lotes por color |
| `GET` | `/api/produccion/historico?horas=24` | Cajas por hora, agrupadas por color |
| `GET` | `/api/salud` | Sin auth. Para confirmar que el proceso está vivo |
| `WS` | `/ws` | Sin auth. Reenvía `intento_login`, `sorter_estado`, `evento_caja`, `alerta` en vivo |

El WebSocket es de solo lectura desde el dashboard: el servidor nunca
espera un mensaje entrante, solo transmite.

**Un solo secreto por usuario.** `usuarios` tiene una sola columna
`password_hash` — pedido del docente: la misma contraseña sirve para
entrar por el teclado 4x4 del ESP32 y por el formulario web. Como el
teclado solo escribe dígitos, el secreto es numérico (un PIN), y esa misma
cadena se escribe en el campo de contraseña de la web.

## Por qué está separado de Docker

Postgres y Mosquitto no cambian de código nunca, así que viven en
contenedores estables. FastAPI sí se edita constantemente durante el
desarrollo — correrlo con `uvicorn` directo (sin reconstruir una imagen
Docker en cada cambio) es más rápido para iterar. En producción/demo esto
podría dockerizarse igual, pero no hacía falta para el plazo del examen.

## Ver también

- [`arquitectura.md`](./arquitectura.md) — dónde corre cada pieza
- [`protocolo.md`](./protocolo.md) — los topics MQTT que consume/publica este servidor, con payloads reales
- [`dashboard.md`](./dashboard.md) — el cliente que consume esta API
