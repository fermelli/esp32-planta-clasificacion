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
│   ├── camara.py             # token de la cámara, carpetas de capturas/modelos, guardar_jpeg, calidad de imagen
│   ├── rostro.py              # cámara A: YuNet + SFace (OpenCV), login PIN + rostro pendiente
│   ├── color_ia.py             # cámara B: características + SVM (scikit-learn), entrenar/predecir
│   ├── ws.py                 # ConnectionManager del WebSocket
│   ├── schemas.py             # modelos Pydantic de entrada/salida
│   └── routers/
│       ├── auth.py             # POST /api/auth/login
│       ├── intentos.py          # GET /api/intentos, /api/alertas
│       ├── comandos.py           # POST /api/comandos
│       ├── produccion.py          # GET /api/eventos, /api/conteos, /api/produccion/historico
│       ├── rostro.py               # /api/rostro/*: verificar, muestra (cámara) y enrolar, usuarios (dashboard)
│       ├── color.py                 # /api/color/*: captura (cámara) y capturas, resumen, entrenar (dashboard)
│       └── camara.py                 # /api/camara/*: config y estado de las placas, foto de prueba
├── schema.sql               # esquema del Bloque 1 (se auto-aplica al crear el contenedor)
├── migrations/               # esquema de bloques siguientes, se auto-aplica igual que schema.sql
├── ml/
│   ├── descargar_modelos.py    # baja YuNet y SFace a modelos/ (una vez, con internet)
│   └── entrenar_color.py        # entrena el clasificador de color desde la terminal
├── capturas/  modelos/       # fotos de la cámara y modelos .onnx/.joblib (no van a git)
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

En Linux/Mac el venv usa `.venv/bin/` en vez de `.venv/Scripts/`.

`--host 0.0.0.0` importa: es lo que permite que el ESP32 (u otra máquina de
la red) le llegue por la IP de esta PC, no solo `localhost`.

Con el servidor arriba, FastAPI expone documentación interactiva (Swagger)
en `http://<ip>:8000/docs` — sirve para probar cualquier endpoint a mano sin
armar el `curl`.

## Variables de entorno

Definidas en `app/config.py` (pydantic-settings), con default si no están en
`.env`:

| Variable                         | Default                                            | Para qué sirve                                                    |
| -------------------------------- | -------------------------------------------------- | ----------------------------------------------------------------- |
| `DATABASE_URL`                   | `postgresql://planta:planta@localhost:5432/planta` | Conexión al Postgres de `docker-compose.yml`                      |
| `MQTT_HOST` / `MQTT_PORT`        | `localhost` / `1883`                               | Dirección del broker Mosquitto                                    |
| `JWT_SECRET`                     | `cambia-esto`                                      | Firma los tokens de sesión — cambiar en cualquier despliegue real |
| `JWT_EXPIRE_MINUTES`             | `480` (8 h)                                        | Vigencia del token del login web y del keypad                     |
| `SEED_USUARIO_1_NOMBRE/_SECRETO` | `operador1` / `1234`                               | Usuario sembrado por `seed.py`                                    |
| `SEED_USUARIO_2_NOMBRE/_SECRETO` | `operador2` / `5678`                               | Segundo usuario sembrado por `seed.py`                            |
| `CAMARA_TOKEN`                   | `cambia-el-token-de-la-camara`                     | Header `X-Camara-Token` con el que se autentica la ESP32-S3-CAM   |
| `LOGIN_ROSTRO`                   | `false`                                            | Versión A: el login del teclado exige PIN + rostro                |
| `ROSTRO_UMBRAL`                  | `0.363`                                            | Similitud coseno mínima para aceptar una cara                     |
| `ROSTRO_VENTANA_S`               | `15`                                               | Segundos que tiene la cámara para resolver el rostro tras el PIN  |
| `ROSTRO_MAX_RECHAZOS`            | `3`                                                | Fotos con otra cara antes de cerrar el intento como fallido       |
| `CAMARA_COLOR`                   | `false`                                            | Versión B: cada caja dispara una foto que clasifica la IA         |
| `COLOR_RECORTE`                  | `0.6`                                              | Fracción central de la foto que mira el clasificador de color     |
| `ROSTRO_WEBCAM`                  | `false`                                            | El servidor saca la foto de rostro con una webcam USB local, sin placa |
| `COLOR_WEBCAM`                   | `false`                                            | Igual que arriba, para la foto de color de cada caja               |
| `WEBCAM_INDICE`                  | `0`                                                | Índice de `cv2.VideoCapture` de la webcam (ver `camara-rostro.md`) |
| `CAPTURAS_DIR` / `MODELOS_DIR`   | `capturas` / `modelos`                             | Carpetas (relativas a `server/`) de las fotos y los modelos       |

## Modelo de datos

![Modelo de datos](img/modelo-datos.svg)

Cuatro tablas base, repartidas entre `schema.sql` (Bloque 1) y
`migrations/002_produccion.sql` (Bloque 2), más tres de la cámara
(`003_rostro.sql` y `004_color_camara.sql`, ver abajo):

| Tabla            | Para qué                       | Quién escribe                                                                                                               |
| ---------------- | ------------------------------ | --------------------------------------------------------------------------------------------------------------------------- |
| `usuarios`       | Los operadores del sistema     | `seed.py` (los 2 iniciales) y `POST /api/usuarios` (altas desde el dashboard) — columna única `password_hash` (ver [`dashboard.md`](./dashboard.md) sobre el mismo secreto en keypad y web) |
| `intentos_login` | Cada login, exitoso o fallido  | `POST /api/auth/login` (origen `web`) y `planta/login/intento` (origen `keypad`)                                            |
| `alertas`        | Avisos para LCD y dashboard    | Login bloqueado (2º intento fallido), lote de 5 cajas completado y cámara desconectada (`camara_offline`)                   |
| `eventos_caja`   | Cada caja que pasa el sensor   | `planta/sorter/evento`; es la tabla central, inventario y dataset a la vez                                                  |

`eventos_caja` trae `color_ml` y `etiqueta_real`. `color_ml` lo llena la
cámara de color (versión B) con lo que predijo la IA; `etiqueta_real`
queda para corregir a mano el color de una caja, y tiene prioridad sobre el
del sensor al entrenar.

Las tablas de la cámara:

| Tabla                   | Para qué                                                         | Quién escribe                |
| ----------------------- | ---------------------------------------------------------------- | ---------------------------- |
| `rostros`               | Embeddings (128 números) de las caras enroladas de cada usuario  | `POST /api/rostro/muestra`   |
| `verificaciones_rostro` | Cada foto con cara verificada durante un login, con su similitud | `POST /api/rostro/verificar` |
| `capturas_camara`       | Una foto por caja + lo que predijo la IA (color, confianza)      | `POST /api/color/captura`    |

### Migraciones

Postgres corre **una sola vez** los `.sql` de `docker-entrypoint-initdb.d`:
al crear el volumen `pgdata` la primera vez que se hace `docker compose up`.
`docker-compose.yml` monta tanto `schema.sql` como
`migrations/002_produccion.sql` ahí, así que si el volumen es nuevo, las dos
se aplican solas y no hace falta correr nada a mano.

Si el volumen ya existía (por ejemplo, se agregó una migración después de
haber levantado el proyecto), hay dos opciones:

```bash
# opción A: aplicar solo la migración nueva a mano
docker compose exec -T postgres psql -U planta -d planta -f /docker-entrypoint-initdb.d/02-produccion.sql
# (igual con 03-rostro.sql y 04-color-camara.sql; en Git Bash de Windows
# anteponer MSYS_NO_PATHCONV=1 o usar PowerShell, si no reescribe la ruta)

# opción B: recrear el volumen desde cero (se pierde todo lo guardado)
docker compose down -v
docker compose up -d
```

## Endpoints REST

Todos menos `/api/salud` y `/api/auth/login` requieren `Authorization: Bearer <token>`.

| Método   | Ruta                                                 | Qué hace                                                                                                                           |
| -------- | ---------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `POST`   | `/api/auth/login`                                    | `{nombre, password}` → JWT. Registra el intento (origen `web`)                                                                     |
| `GET`    | `/api/usuarios`                                      | Lista los operadores registrados (id, nombre, fecha de alta)                                                                       |
| `POST`   | `/api/usuarios`                                      | `{nombre, password}` → registra un operador nuevo. `password` numérico (1-8 dígitos, igual que el teclado); 409 si el nombre ya existe |
| `GET`    | `/api/intentos?limite=50`                            | Historial de login (`keypad` + `web`)                                                                                              |
| `GET`    | `/api/alertas?limite=50`                             | Alertas (`login_bloqueado`, `lote_completo`, `camara_offline`)                                                                     |
| `POST`   | `/api/comandos`                                      | `{cmd, arg}` → publica en `planta/cmd`. `cmd`: `puerta`, `motor`, `reset_counts`                                                   |
| `GET`    | `/api/eventos?limite=50`                             | Últimas cajas, con lectura cruda del sensor                                                                                        |
| `GET`    | `/api/conteos`                                       | Conteo actual + total histórico + lotes por color                                                                                  |
| `GET`    | `/api/produccion/historico?horas=24`                 | Cajas por hora, agrupadas por color                                                                                                |
| `GET`    | `/api/salud`                                         | Sin auth. Para confirmar que el proceso está vivo                                                                                  |
| `WS`     | `/ws`                                                | Sin auth. Reenvía `intento_login`, `sorter_estado`, `evento_caja`, `alerta`, `verificacion_rostro`, `clasificacion_camara` en vivo |
| `GET`    | `/capturas/<ruta>`                                   | Sin auth. Las fotos de la cámara (un `<img>` no puede mandar el JWT)                                                               |
| `POST`   | `/api/rostro/verificar`, `/muestra`                  | Cámara, con `X-Camara-Token`. Foto JPEG en el cuerpo (ver [`protocolo.md`](./protocolo.md))                                        |
| `GET`    | `/api/camara/config`                                 | Por versión de cámara: `flag` (activada en el `.env`), `online` (hay placa conectada) y `activo` (las dos)                         |
| `POST`   | `/api/camara/probar/{version}`                       | Pide una foto de prueba a la placa (`rostro` o `color`); 409 si no está conectada. Con `*_WEBCAM=true` la captura el servidor mismo y 502 si la webcam no responde. El resultado llega por WS como `prueba_camara` |
| `GET`    | `/api/camara/prueba/{version}`                       | Última foto de prueba con sus métricas (brillo, nitidez, caras o color), o `null`                                                  |
| `POST`   | `/api/camara/prueba`                                 | Placa, con `X-Camara-Token`. La foto de prueba (`?version=`); guarda `capturas/prueba/<version>.jpg`                               |
| `GET`    | `/api/rostro/config`, `/usuarios`, `/verificaciones` | Estado del login por rostro, muestras por usuario, verificaciones recientes                                                        |
| `POST`   | `/api/rostro/enrolar/{usuario_id}`                   | Ordena a la cámara (o a la webcam local, con `ROSTRO_WEBCAM=true`) tomar 5 fotos de ese usuario                                    |
| `GET`    | `/api/rostro/muestras/{usuario_id}`                  | Las fotos guardadas al enrolar a ese usuario, más recientes primero                                                                |
| `DELETE` | `/api/rostro/muestras/{usuario_id}`                  | Borra las muestras de rostro de un usuario                                                                                         |
| `POST`   | `/api/color/captura`                                 | Cámara, con `X-Camara-Token`. Foto de una caja (`?evento_id=`)                                                                     |
| `GET`    | `/api/color/capturas`, `/resumen`                    | Fotos con sensor vs. IA; % de acuerdo, dataset por color y métricas del modelo                                                     |
| `POST`   | `/api/color/entrenar`                                | Entrena el clasificador con las fotos guardadas y devuelve accuracy y matriz de confusión                                          |

El WebSocket es de solo lectura desde el dashboard: el servidor nunca
espera un mensaje entrante, solo transmite.

**Un solo secreto por usuario.** `usuarios` tiene una sola columna
`password_hash` — pedido del docente: la misma contraseña sirve para
entrar por el teclado 4x4 del ESP32 y por el formulario web. Como el
teclado solo escribe dígitos, el secreto es numérico (un PIN), y esa misma
cadena se escribe en el campo de contraseña de la web.

### Ejemplos de respuesta

`POST /api/auth/login` con `{nombre, password}` correctos:

```json
{
  "access_token": "eyJhbGciOi...",
  "token_type": "bearer",
  "nombre": "operador1"
}
```

Con credenciales incorrectas devuelve `401` con
`{"detail": "Usuario o contraseña incorrectos"}`; un token vencido o mal
formado en cualquier otro endpoint también devuelve `401`, esta vez con
`{"detail": "Token inválido o expirado"}` (lo arma `deps.get_current_user`).

`GET /api/conteos` calcula `conteo_actual` tomando, por color, la última fila
de `eventos_caja` (`DISTINCT ON` ordenado por fecha) — es el mismo número que
ya calculó el ESP32 para prender los LEDs binarios. `total_historico` y
`lotes_completados` sí se agregan sobre toda la tabla:

```json
[
  {
    "color": "rojo",
    "conteo_actual": 3,
    "total_historico": 18,
    "lotes_completados": 3
  }
]
```

`GET /api/produccion/historico?horas=24` devuelve una fila por combinación
hora/color (`date_trunc('hour', creado_en)`), pensada para graficar barras
agrupadas — así la consume [`HistoricoView.vue`](./dashboard.md):

```json
[{ "hora": "2026-09-23T14:00:00Z", "color": "rojo", "cantidad": 4 }]
```

`POST /api/comandos` no valida el valor de `cmd`: publica `{cmd, arg}` tal
cual en `planta/cmd`, y es el gateway (ESP32 #1) quien decide qué hacer con
un comando desconocido (ver [`protocolo.md`](./protocolo.md)).

## Qué hace el servidor con cada mensaje MQTT

`app/mqtt_client.py` corre paho-mqtt en su propio hilo (`loop_start()`) y
cruza al event loop de asyncio con `run_coroutine_threadsafe` — así los
handlers pueden usar el mismo pool de Postgres y el mismo
`ConnectionManager` del WebSocket que usan los endpoints REST.

- **`planta/login/intento`** — `{"pin": "1234", "intento": 1}`. Compara el
  PIN contra el `password_hash` de **todos** los usuarios con `bcrypt.checkpw`
  (en `asyncio.to_thread`, porque bcrypt tarda 100-300ms y es CPU-bound: hacerlo
  en el hilo principal congelaría MQTT y WebSocket mientras tanto). Inserta la
  fila en `intentos_login` (origen `keypad`); si es el 2º intento fallido,
  además inserta una alerta `login_bloqueado`. Responde en
  `planta/login/resultado` y lo retransmite por WS como `intento_login`.
  Con `LOGIN_ROSTRO=true` y el PIN correcto, en cambio, deja el login
  pendiente y le pide la cara a la cámara (ver [`camara-rostro.md`](./camara-rostro.md));
  el resultado sale recién cuando la cara se resolvió. Si **no hay cámara de
  rostro conectada**, el login entra solo con el PIN y queda una alerta
  `camara_offline`: el teclado nunca se bloquea por una placa caída. Con
  `ROSTRO_WEBCAM=true` no espera ninguna placa: captura unos pocos frames
  locales ahí mismo (y si la webcam no responde, el intento se cierra como
  fallido por rostro, no como "sin cámara" — ver la nota en
  [`camara-rostro.md`](./camara-rostro.md#con-webcam-usb-en-vez-de-la-esp32-cam)).
- **`planta/sorter/estado`** — se reenvía tal cual por WebSocket
  (`sorter_estado`) y no se persiste: es estado instantáneo del hardware
  (puerta, cinta), no un evento con historia.
- **`planta/sorter/evento`** — cada caja que pasa el sensor. Inserta la fila
  completa en `eventos_caja` (color, conteo, lectura cruda del sensor) y la
  retransmite como `evento_caja`. Si trae `lote_completo: true`, además
  inserta una alerta `lote_completo` y manda un WS `alerta` aparte. Con
  `CAMARA_COLOR=true` **y una cámara de color conectada** publica además
  `planta/camara/capturar` con el `id` de la fila, para que la cámara saque
  la foto de esa caja. Con `COLOR_WEBCAM=true` no publica nada: captura la
  foto ahí mismo con la webcam y la procesa directo.
- **`planta/camara/estado/rostro`** y **`.../color`** — el saludo de cada
  firmware de cámara. El servidor guarda cuáles están conectadas, lo
  retransmite por WS como `camara_estado` y, si una versión activada por flag
  se desconecta, inserta una alerta `camara_offline`.
- **`planta/sorter/alerta`** — inserta la alerta con el `tipo`/`mensaje` que
  venga y la retransmite. Sin uso todavía por parte del sorter, pero el
  camino ya está listo.

### Mensajes que llegan por el WebSocket

Los `type` que puede recibir `/ws`, con la forma real del JSON:

```json
{ "type": "intento_login", "exito": true, "nombre": "operador1", "bloqueado": false, "intento": 1 }
{ "type": "sorter_estado", "puerta_abierta": true, "cinta_estado": "low" }
{ "type": "evento_caja", "color": "rojo", "conteo": 3, "lote_completo": false, "r": 900, "g": 200, "b": 180, "c": 1300 }
{ "type": "alerta", "tipo": "lote_completo", "mensaje": "Lote de 5 cajas rojo completado" }
{ "type": "camara_estado", "version": "rostro", "online": true }
{ "type": "prueba_camara", "version": "rostro", "imagen": "prueba/rostro.jpg", "brillo": 124.2, "nitidez": 387.1, "avisos": [], "rostro": { "caras": 1, "coincidencias": [] }, "color": null }
{ "type": "verificacion_rostro", "usuario_id": 1, "nombre": "operador1", "similitud": 0.92, "exito": true, "umbral": 0.363, "imagen": "rostro/v1_1759000000000.jpg" }
{ "type": "clasificacion_camara", "evento_id": 42, "color_sensor": "rojo", "color_ia": "rojo", "confianza": 0.87, "coincide": true, "imagen": "color/42.jpg" }
```

## Seguridad (alcance de laboratorio)

A propósito relajada, porque el proyecto vive en la red del laboratorio y no
sale a internet:

- **CORS abierto** (`allow_origins=["*"]`) — cualquier origen puede llamar a
  la API.
- **Mosquitto sin usuario ni TLS** (`allow_anonymous true` en
  `mosquitto/mosquitto.conf`) — cualquiera en la red puede publicar o
  suscribirse a los topics.
- **`/ws` sin autenticación** — el WebSocket no pide token; solo transmite,
  nunca ejecuta comandos.
- **`/capturas/` sin autenticación** — las fotos de la cámara (incluidas las
  de las caras) las puede pedir cualquiera que conozca el nombre del archivo.
- **`seed.py` imprime la clave sembrada** por consola, para poder copiarla al
  keypad del ESP32 sin ir a buscarla en el `.env`.

## Por qué está separado de Docker

Postgres y Mosquitto no cambian de código nunca, así que viven en
contenedores estables. FastAPI sí se edita constantemente durante el
desarrollo — correrlo con `uvicorn` directo (sin reconstruir una imagen
Docker en cada cambio) es más rápido para iterar. En producción/demo esto
podría dockerizarse igual, pero no hacía falta para el plazo del examen.

## Problemas comunes

- **`uvicorn` no arranca o se cuelga al iniciar**: `mqtt_client.start()` se
  conecta al broker de forma síncrona dentro del `lifespan`, así que si
  Mosquitto (o Postgres) todavía no están arriba (`docker compose up -d`
  recién ejecutado), el arranque falla o queda esperando. Confirmar con
  `docker compose ps` que ambos estén `healthy`/`running` antes de levantar
  FastAPI.
- **El ESP32 no logra conectarse**: casi siempre falta el `--host 0.0.0.0`
  al correr `uvicorn` (por default solo escucha en `localhost`), o el
  firewall de Windows está bloqueando el puerto 8000 (API/WS) o el 1883
  (MQTT) para conexiones entrantes desde la red local.

  Caso puntual ya visto: Docker Desktop crea sola una regla de firewall
  ("Docker Desktop Backend") para el puerto de Mosquitto, pero esa regla
  viene con el perfil de red limitado a **Público** — si tu red WiFi/Ethernet
  está en modo **Privado** (lo normal en casa), la regla no aplica y el
  ESP32 nunca completa la conexión TCP al 1883 aunque el contenedor esté
  arriba. Se soluciona agregando una regla propia:

  ```powershell
  New-NetFirewallRule -DisplayName "MQTT 1883 (planta)" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow -Profile Private,Domain
  ```

  (requiere PowerShell como Administrador). Revisa el perfil de tu red con
  `Get-NetConnectionProfile`.

## Ver también

- [`arquitectura.md`](./arquitectura.md) — dónde corre cada pieza
- [`protocolo.md`](./protocolo.md) — los topics MQTT que consume/publica este servidor, con payloads reales
- [`dashboard.md`](./dashboard.md) — el cliente que consume esta API
- [`camara-rostro.md`](./camara-rostro.md) y [`camara-color.md`](./camara-color.md) — la cámara con IA
