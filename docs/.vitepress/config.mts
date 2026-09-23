import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Planta de clasificación',
  description: 'Documentación técnica — COM520 primer parcial',
  lang: 'es',
  cleanUrls: true,
  lastUpdated: true,
  // Enlaces intencionales al código fuente fuera de docs/ (../firmware/...)
  // y al directorio img/ (solo contiene SVG, sin index.md) — no son errores.
  ignoreDeadLinks: true,

  themeConfig: {
    nav: [
      { text: 'Inicio', link: '/' },
      { text: 'Arquitectura', link: '/arquitectura' },
      { text: 'Protocolo', link: '/protocolo' },
    ],

    sidebar: [
      {
        text: 'Sistema',
        items: [
          { text: 'Arquitectura', link: '/arquitectura' },
          { text: 'Protocolo (ESP-NOW / MQTT)', link: '/protocolo' },
        ],
      },
      {
        text: 'Software',
        items: [
          { text: 'Servidor (FastAPI)', link: '/servidor' },
          { text: 'Dashboard (Vue)', link: '/dashboard' },
        ],
      },
      {
        text: 'Conexiones',
        items: [
          { text: 'ESP32 #1 — gateway', link: '/conexiones-esp32-gateway' },
          { text: 'ESP32 #2 — sorter', link: '/conexiones-esp32-sorter' },
        ],
      },
    ],

    outline: { level: [2, 3], label: 'En esta página' },

    socialLinks: [],

    search: { provider: 'local' },
  },
})
