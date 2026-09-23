# Dashboard (Vue)

`dashboard/` es el cliente web: Vue 3 + TypeScript + Vite, Tailwind v4 y
componentes estilo shadcn-vue armados a mano (la CLI de esa librería no
corre con Node 20 y, forzando una versión anterior, se rompe igual contra
su propio registry remoto — ver el commit que los agrega para el detalle).
Pinia para estado, vue-router para las páginas.

## Estructura

```
dashboard/
├── src/
│   ├── App.vue                # layout: nav + badge de conexión + logout
│   ├── main.ts
│   ├── style.css               # variables de tema (Tailwind v4, oklch)
│   ├── lib/
│   │   ├── api.ts                # apiFetch() — agrega el JWT, maneja 401
│   │   ├── types.ts               # tipos de las respuestas del backend
│   │   └── utils.ts                # cn() (clsx + tailwind-merge)
│   ├── stores/
│   │   ├── auth.ts                 # login, JWT en localStorage
│   │   └── live.ts                  # WebSocket con reconexión automática
│   ├── router/index.ts               # guard: sin token → /login
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

## Cómo llegan los datos

Cada vista pide su estado inicial por REST al entrar (`onMounted`) y
después se mantiene sola: el store `live` abre un WebSocket a `/ws` una
sola vez (desde `App.vue`, al iniciar sesión) y cada vista que le interesa
un tipo de evento lo mira con un `watch` sobre el store — así un evento de
producción, una alerta o un intento de login aparecen sin que el usuario
tenga que recargar la página.

## Ver también

- [`servidor.md`](./servidor.md) — la API que este dashboard consume, con
  todos los endpoints y el WebSocket
- [`protocolo.md`](./protocolo.md) — la forma exacta de los mensajes en vivo
