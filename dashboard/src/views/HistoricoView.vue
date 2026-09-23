<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { apiFetch } from '@/lib/api'
import type { CajaPorHora } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card'
import { History, ChartNoAxesColumn } from 'lucide-vue-next'

const COLORES = [
  { id: 'rojo', hex: '#ef4444' },
  { id: 'verde', hex: '#059669' },
  { id: 'azul', hex: '#3b82f6' },
] as const

const datos = ref<CajaPorHora[]>([])
const cargando = ref(true)

onMounted(async () => {
  try {
    datos.value = await apiFetch<CajaPorHora[]>('/api/produccion/historico?horas=24')
  } finally {
    cargando.value = false
  }
})

const horas = computed(() => [...new Set(datos.value.map((d) => d.hora))].sort())

const maxCantidad = computed(() => Math.max(1, ...datos.value.map((d) => d.cantidad)))

function cantidad(hora: string, color: string): number {
  return datos.value.find((d) => d.hora === hora && d.color === color)?.cantidad ?? 0
}

// Layout del gráfico
const ANCHO = 900
const ALTO = 320
const MARGEN = { top: 20, right: 20, bottom: 50, left: 40 }
const anchoGrafico = ANCHO - MARGEN.left - MARGEN.right
const altoGrafico = ALTO - MARGEN.top - MARGEN.bottom

const grupoAncho = computed(() => (horas.value.length ? anchoGrafico / horas.value.length : 0))
const barraAncho = computed(() => Math.max(2, grupoAncho.value / (COLORES.length + 1)))

function alturaBarra(valor: number): number {
  return (valor / maxCantidad.value) * altoGrafico
}

function formatoHora(iso: string): string {
  return new Date(iso).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
}

// Líneas guía horizontales: 0, mitad y máximo.
const lineasGuia = computed(() =>
  [0, 0.5, 1].map((f) => ({
    y: altoGrafico - f * altoGrafico,
    valor: Math.round(maxCantidad.value * f),
  })),
)
</script>

<template>
  <Card>
    <CardHeader>
      <CardTitle class="flex items-center gap-2 text-foreground">
        <History class="h-4 w-4 text-muted-foreground" stroke-width="2" />
        Cajas por hora (últimas 24h)
      </CardTitle>
      <CardDescription>Una barra por color, agrupadas por hora.</CardDescription>
    </CardHeader>
    <CardContent>
      <div v-if="cargando" class="h-[320px] w-full animate-pulse rounded-md bg-muted" />
      <div
        v-else-if="horas.length === 0"
        class="flex flex-col items-center gap-2 py-14 text-muted-foreground"
      >
        <ChartNoAxesColumn class="h-8 w-8" stroke-width="1.5" />
        <span>Todavía no hay eventos registrados.</span>
      </div>
      <figure v-else class="overflow-x-auto">
        <svg
          :viewBox="`0 0 ${ANCHO} ${ALTO}`"
          role="img"
          aria-label="Cajas clasificadas por hora, una barra por color"
          class="w-full min-w-[600px]"
        >
          <g :transform="`translate(${MARGEN.left},${MARGEN.top})`">
            <g v-for="linea in lineasGuia" :key="linea.y">
              <line
                x1="0"
                :y1="linea.y"
                :x2="anchoGrafico"
                :y2="linea.y"
                stroke="currentColor"
                class="text-border"
              />
              <text
                x="-8"
                :y="linea.y"
                text-anchor="end"
                dominant-baseline="middle"
                font-size="10"
                fill="currentColor"
                class="text-muted-foreground tabular-nums"
              >
                {{ linea.valor }}
              </text>
            </g>

            <g
              v-for="(hora, hi) in horas"
              :key="hora"
              :transform="`translate(${hi * grupoAncho},0)`"
            >
              <rect
                v-for="(color, ci) in COLORES"
                :key="color.id"
                class="transition-opacity hover:opacity-80"
                :x="ci * barraAncho + barraAncho / 2"
                :y="altoGrafico - alturaBarra(cantidad(hora, color.id))"
                :width="barraAncho"
                :height="alturaBarra(cantidad(hora, color.id))"
                :fill="color.hex"
                rx="3"
              >
                <title>
                  {{ color.id }} · {{ formatoHora(hora) }} · {{ cantidad(hora, color.id) }} cajas
                </title>
              </rect>
              <text
                :x="grupoAncho / 2"
                :y="altoGrafico + 18"
                text-anchor="middle"
                font-size="10"
                fill="currentColor"
                class="text-muted-foreground"
              >
                {{ formatoHora(hora) }}
              </text>
            </g>
          </g>
        </svg>
        <figcaption class="mt-2 flex justify-center gap-4 text-xs text-muted-foreground">
          <span
            v-for="color in COLORES"
            :key="color.id"
            class="flex items-center gap-1.5 capitalize"
          >
            <span class="h-2.5 w-2.5 rounded-full" :style="{ backgroundColor: color.hex }" />
            {{ color.id }}
          </span>
        </figcaption>
      </figure>
    </CardContent>
  </Card>
</template>
