# Dashboard (Vue)

`dashboard/` es el cliente web: Vue 3 + TypeScript + Vite, Tailwind v4 y
componentes estilo shadcn-vue armados a mano sobre reka-ui (la CLI de esa
librería no corre con Node 20 y, forzando una versión anterior, se rompe
igual contra su propio registry remoto — ver el commit que los agrega para
el detalle). Pinia para estado, vue-router para las páginas.

## Estructura

```
dashboard/
├── src/
│   ├── App.vue                # layout: nav con íconos, badge de conexión, toggle de tema, logout
│   ├── main.ts
│   ├── style.css               # variables de tema (Tailwind v4, oklch)
│   ├── lib/
│   │   ├── api.ts                # apiFetch() — agrega el JWT, maneja 401
│   │   ├── progreso.ts            # ref() global que alimenta la barra de progreso del router
│   │   ├── types.ts               # tipos de las respuestas del backend
│   │   └── utils.ts                # cn() (clsx + tailwind-merge)
│   ├── stores/
│   │   ├── auth.ts                 # login, JWT en localStorage
│   │   ├── live.ts                  # WebSocket con reconexión automática
│   │   └── theme.ts                  # modo claro/oscuro, persistido en localStorage
│   ├── router/
│   │   ├── index.ts                   # guard: sin token → /login
│   │   └── types.d.ts                  # tipa route.meta.publica
│   ├── components/
│   │   ├── ui/                        # button, card, input, label, badge, table
│   │   └── ContadorBinario.vue         # los 3 LEDs binarios del hardware, en pantalla
│   └── views/
│       ├── LoginView.vue
│       ├── ProduccionView.vue           # conteos en vivo, cinta/puerta, últimas cajas
│       ├── HistoricoView.vue             # cajas por hora (gráfico SVG a mano)
│       ├── IntentosView.vue
│       └── AlertasView.vue
└── .env.example
```

## Cómo correrlo

```bash
cd dashboard
cp .env.example .env.local   # VITE_API_URL / VITE_WS_URL → IP de la PC de escritorio
npm install
npm run dev
```

Necesita el servidor (`server/`) corriendo — ver
[`servidor.md`](./servidor.md).

Otros scripts: `npm run build` (chequeo de tipos con `vue-tsc` + build de
Vite), `npm run lint` (ESLint con `--fix`) y `npm run format` (Prettier sobre
`src/`).

### Variables de entorno

`.env.example` trae solo dos, apuntando a `localhost` por default:

| Variable       | Ejemplo                  | Cuándo cambiarla                                                                        |
| -------------- | ------------------------ | --------------------------------------------------------------------------------------- |
| `VITE_API_URL` | `http://localhost:8000`  | Si el dashboard se abre desde otra máquina, usar la IP de la PC donde corre el servidor |
| `VITE_WS_URL`  | `ws://localhost:8000/ws` | Misma IP que `VITE_API_URL`, con esquema `ws://` y el path `/ws`                        |

No usar túneles de VS Code acá — ver [`arquitectura.md`](./arquitectura.md)
sobre por qué.

## Rutas y vistas

Todas las rutas salvo `/login` están protegidas por el guard de
`router/index.ts` (sin `auth.token` → redirige a `/login?next=<ruta>`).

| Ruta         | Vista            | Carga inicial (REST)                              | Se actualiza en vivo con...                      |
| ------------ | ---------------- | ------------------------------------------------- | ------------------------------------------------ |
| `/login`     | `LoginView`      | `POST /api/auth/login`                            | —                                                |
| `/`          | `ProduccionView` | `GET /api/conteos` + `GET /api/eventos?limite=15` | `evento_caja`, `sorter_estado`                   |
| `/historico` | `HistoricoView`  | `GET /api/produccion/historico?horas=24`          | No se actualiza sola; hay que recargar la página |
| `/intentos`  | `IntentosView`   | `GET /api/intentos?limite=30`                     | `intento_login`                                  |
| `/alertas`   | `AlertasView`    | `GET /api/alertas?limite=30`                      | `alerta`                                         |

`ProduccionView` además tiene los controles de planta: botones de cinta
(apagada/baja/full) y de puerta, y un botón de reset de contadores — los
tres mandan `POST /api/comandos` con el `cmd`/`arg` correspondiente (ver la
tabla de comandos en [`protocolo.md`](./protocolo.md)).

## Cómo llegan los datos

Cada vista pide su estado inicial por REST al entrar (`onMounted`) y lo
combina con el buffer en vivo del store `live` (WebSocket a `/ws`, abierto
una sola vez desde `App.vue` al iniciar sesión). Ese store guarda como
máximo los últimos 30 mensajes de cada tipo (`intento_login`, `alerta`,
`evento_caja`) y el último `sorter_estado` recibido.

El patrón varía un poco según la vista:

- **`IntentosView`** y **`AlertasView`** arman la lista con un `computed`
  que concatena "lo que llegó por WS" delante de "lo que trajo el REST
  inicial" y recorta al límite — así lo nuevo aparece arriba sin duplicar
  nada ni esperar una recarga.
- **`ProduccionView`** hace lo mismo para la tabla de últimas cajas, pero
  además tiene un `watch` sobre `live.ultimosEventos` que recalcula
  `conteo_actual`/`total_historico`/`lotes_completados` en el momento en que
  llega cada evento, en vez de solo mostrar el mensaje crudo.
- **`HistoricoView`** no escucha el WebSocket: el gráfico de cajas por hora
  se arma una sola vez al entrar a la vista.

El WebSocket se reconecta solo si se cae: espera 1 segundo la primera vez y
duplica la espera en cada intento fallido hasta un tope de 15 segundos
(`live.ts`). El badge "en vivo"/"desconectado" del header refleja ese
estado en tiempo real.

## Autenticación

El JWT que devuelve `/api/auth/login` se guarda en `localStorage`
(`planta_token`, `planta_nombre`) para sobrevivir un refresh de página; si
el navegador bloquea `localStorage` (modo privado), la sesión sigue
funcionando en memoria hasta que se cierra la pestaña. `apiFetch` agrega el
header `Authorization: Bearer <token>` a cada pedido y, si el servidor
responde `401` (token vencido o inválido), llama a `cerrarSesion()` y corta
la promesa — el efecto visible es volver a `/login`. El token dura 8 horas
(`JWT_EXPIRE_MINUTES` del servidor, ver [`servidor.md`](./servidor.md)).

## Interfaz

- **Tema claro/oscuro** (`stores/theme.ts`): arranca según
  `prefers-color-scheme` si no hay preferencia guardada, alterna la clase
  `dark` en `<html>` y persiste la elección en `localStorage`.
- **Barra de progreso de navegación** (`lib/progreso.ts` + `App.vue`): un
  `ref()` fuera de Pinia (el router la necesita antes de que exista el árbol
  de componentes) que crece al 75% mientras se resuelve la navegación —
  incluida la descarga del chunk lazy de la vista — y remata a 100% al
  terminar, estilo nprogress.
- **Estados de carga**: cada vista muestra skeletons (`animate-pulse`)
  mientras espera su primer REST, y `App.vue` envuelve las vistas en
  `<Suspense>` para el cambio de ruta.
- **Componentes `ui/`**: button, card, input, label, badge y table, escritos
  a mano siguiendo el estilo shadcn-vue pero usando primitivas de reka-ui
  en vez de la CLI oficial.

## Ver también

- [`servidor.md`](./servidor.md) — la API que este dashboard consume, con
  todos los endpoints y el WebSocket
- [`protocolo.md`](./protocolo.md) — la forma exacta de los mensajes en vivo
