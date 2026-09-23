import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Planta de clasificación',
  description: 'Documentación técnica — COM520 primer parcial',
  lang: 'es',
  base: '/esp32-planta-clasificacion/',
  cleanUrls: true,
  lastUpdated: true,
  // Enlaces intencionales al código fuente fuera de docs/ (../firmware/...)
  // y al directorio img/ (solo contiene SVG, sin index.md) — no son errores.
  ignoreDeadLinks: true,

  themeConfig: {
    nav: [
      { text: 'Inicio', link: '/' },
      { text: 'Guía de armado', link: '/guia-armado' },
      { text: 'Arquitectura', link: '/arquitectura' },
      { text: 'Protocolo', link: '/protocolo' },
    ],

    sidebar: [
      {
        text: 'Armado',
        items: [{ text: 'Guía completa (todo junto)', link: '/guia-armado' }],
      },
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
        text: 'Conexiones (por placa)',
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
