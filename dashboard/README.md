# Dashboard — Planta de clasificación

Vue 3 + TypeScript + Vite, Tailwind v4 y componentes estilo shadcn-vue
(armados a mano, sin la CLI — ver el commit que los agrega). Pinia para
estado, vue-router para las páginas.

## Desarrollo

```bash
cp .env.example .env.local   # apuntar a la IP del servidor (ver docs/arquitectura.md)
npm install
npm run dev
```

Requiere el backend (`server/`) corriendo — ver [`../README.md`](../README.md).

## Páginas

- `/login` — JWT
- `/` — producción: contadores binarios en vivo, estado de cinta/puerta, comandos
- `/historico` — cajas por hora, por color
- `/intentos` — accesos (teclado + web)
- `/alertas` — lote completo, login bloqueado
