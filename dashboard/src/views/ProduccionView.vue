<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { ConteoColor, EventoCaja } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import {
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
} from '@/components/ui/table'
import ContadorBinario from '@/components/ContadorBinario.vue'
import {
  SlidersHorizontal,
  ListOrdered,
  Package,
  Power,
  Gauge,
  Zap,
  DoorOpen,
  DoorClosed,
  RotateCcw,
  Inbox,
} from 'lucide-vue-next'

const live = useLiveStore()

const COLORES = [
  { id: 'rojo', label: 'Rojo', hex: '#ef4444' },
  { id: 'verde', label: 'Verde', hex: '#059669' },
  { id: 'azul', label: 'Azul', hex: '#3b82f6' },
] as const

const CINTA_OPCIONES = [
  { arg: 0, valor: 'off' as const, label: 'Apagada', icon: Power },
  { arg: 1, valor: 'low' as const, label: 'Baja', icon: Gauge },
  { arg: 2, valor: 'full' as const, label: 'Full', icon: Zap },
]

const conteos = ref<Record<string, ConteoColor>>({})
const eventosIniciales = ref<EventoCaja[]>([])
const enviandoComando = ref<string | null>(null)
const cargando = ref(true)

async function cargar() {
  cargando.value = true
  try {
    const [c, e] = await Promise.all([
      apiFetch<ConteoColor[]>('/api/conteos'),
      apiFetch<EventoCaja[]>('/api/eventos?limite=15'),
    ])
    conteos.value = Object.fromEntries(c.map((x) => [x.color, x]))
    eventosIniciales.value = e
  } finally {
    cargando.value = false
  }
}

onMounted(cargar)

// Cuando llega un evento en vivo, actualiza el conteo de ese color sin esperar
// a la próxima recarga — es la misma cifra que el hardware ya calculó.
watch(
  () => live.ultimosEventos,
  (eventos) => {
    const ultimo = eventos[0]
    if (!ultimo) return
    const previo = conteos.value[ultimo.color]
    conteos.value = {
      ...conteos.value,
      [ultimo.color]: {
        color: ultimo.color,
        conteo_actual: ultimo.conteo,
        total_historico: (previo?.total_historico ?? 0) + 1,
        lotes_completados: (previo?.lotes_completados ?? 0) + (ultimo.lote_completo ? 1 : 0),
      },
    }
  },
  { deep: true },
)

const filasTabla = computed(() => {
  const vivos = live.ultimosEventos.map((e) => ({ ...e, creado_en: new Date().toISOString() }))
  return [...vivos, ...eventosIniciales.value].slice(0, 15)
})

const totalProcesado = computed(() =>
  Object.values(conteos.value).reduce((acc, c) => acc + c.total_historico, 0),
)

async function enviarComando(cmd: string, arg = 0) {
  enviandoComando.value = `${cmd}:${arg}`
  try {
    await apiFetch('/api/comandos', { method: 'POST', body: JSON.stringify({ cmd, arg }) })
  } finally {
    enviandoComando.value = null
  }
}
</script>

<template>
  <div class="flex flex-col gap-6">
    <div class="grid grid-cols-1 gap-4 sm:grid-cols-3">
      <Card
        v-for="color in COLORES"
        :key="color.id"
        class="overflow-hidden border-t-2"
        :style="{ borderTopColor: color.hex }"
      >
        <CardHeader class="pb-1">
          <CardTitle class="flex items-center justify-between text-sm font-medium text-muted-foreground">
            <span class="flex items-center gap-2">
              <span
                class="flex h-6 w-6 items-center justify-center rounded-md"
                :style="{ backgroundColor: `${color.hex}1a`, color: color.hex }"
              >
                <Package class="h-3.5 w-3.5" stroke-width="2" />
              </span>
              {{ color.label }}
            </span>
            <Badge v-if="conteos[color.id]?.lotes_completados" variant="outline" class="text-[11px]">
              {{ conteos[color.id]?.lotes_completados }} lote{{ conteos[color.id]!.lotes_completados === 1 ? '' : 's' }}
            </Badge>
          </CardTitle>
        </CardHeader>
        <CardContent class="flex items-end justify-between pt-1">
          <div v-if="cargando" class="h-9 w-16 animate-pulse rounded-md bg-muted" />
          <p v-else class="text-4xl font-bold tabular-nums tracking-tight text-foreground">
            {{ conteos[color.id]?.total_historico ?? 0 }}
          </p>
          <ContadorBinario
            :valor="conteos[color.id]?.conteo_actual ?? 0"
            :color-encendido="color.hex"
            label="LEDs"
          />
        </CardContent>
      </Card>
    </div>

    <Card>
      <CardHeader>
        <CardTitle class="flex items-center gap-2">
          <SlidersHorizontal class="h-4 w-4 text-muted-foreground" stroke-width="2" />
          Cinta y puerta
        </CardTitle>
      </CardHeader>
      <CardContent class="flex flex-wrap items-center gap-4">
        <div class="flex flex-wrap items-center gap-4">
          <div class="flex items-center gap-1 rounded-lg bg-muted p-1">
            <Button
              v-for="opcion in CINTA_OPCIONES"
              :key="opcion.arg"
              size="sm"
              variant="ghost"
              class="gap-1.5"
              :class="
                live.sorterEstado.cinta_estado === opcion.valor
                  ? '!bg-card !text-foreground shadow-sm'
                  : ''
              "
              :disabled="!!enviandoComando"
              @click="enviarComando('motor', opcion.arg)"
            >
              <component :is="opcion.icon" class="h-3.5 w-3.5" stroke-width="2" />
              {{ opcion.label }}
            </Button>
          </div>

          <div class="flex items-center gap-1 rounded-lg bg-muted p-1">
            <Button
              size="sm"
              variant="ghost"
              class="gap-1.5"
              :class="live.sorterEstado.puerta_abierta === true ? '!bg-card !text-foreground shadow-sm' : ''"
              :disabled="!!enviandoComando"
              @click="enviarComando('puerta', 1)"
            >
              <DoorOpen class="h-3.5 w-3.5" stroke-width="2" />
              Abrir
            </Button>
            <Button
              size="sm"
              variant="ghost"
              class="gap-1.5"
              :class="live.sorterEstado.puerta_abierta === false ? '!bg-card !text-foreground shadow-sm' : ''"
              :disabled="!!enviandoComando"
              @click="enviarComando('puerta', 0)"
            >
              <DoorClosed class="h-3.5 w-3.5" stroke-width="2" />
              Cerrar
            </Button>
          </div>
        </div>

        <Button
          size="sm"
          variant="destructive"
          class="ml-auto gap-1.5"
          :disabled="!!enviandoComando"
          @click="enviarComando('reset_counts')"
        >
          <RotateCcw class="h-3.5 w-3.5" stroke-width="2" />
          Reset contadores
        </Button>
      </CardContent>
    </Card>

    <Card>
      <CardHeader>
        <CardTitle class="flex items-center justify-between">
          <span class="flex items-center gap-2">
            <ListOrdered class="h-4 w-4 text-muted-foreground" stroke-width="2" />
            Últimas cajas
          </span>
          <span class="text-xs font-normal text-muted-foreground">{{ totalProcesado }} en total</span>
        </CardTitle>
      </CardHeader>
      <CardContent>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Color</TableHead>
              <TableHead>Conteo</TableHead>
              <TableHead>Lote</TableHead>
              <TableHead>RGB / Clear</TableHead>
              <TableHead>Hora</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow v-for="(fila, i) in filasTabla" :key="i">
              <TableCell class="capitalize">
                <span class="flex items-center gap-2">
                  <span
                    class="h-2 w-2 rounded-full"
                    :style="{
                      backgroundColor: COLORES.find((c) => c.id === fila.color)?.hex ?? '#999',
                    }"
                  />
                  {{ fila.color }}
                </span>
              </TableCell>
              <TableCell class="tabular-nums">{{ fila.conteo }}</TableCell>
              <TableCell>
                <Badge v-if="fila.lote_completo" variant="success">completo</Badge>
              </TableCell>
              <TableCell class="text-xs text-muted-foreground tabular-nums"
                >{{ fila.r }}/{{ fila.g }}/{{ fila.b }} · {{ fila.c }}</TableCell
              >
              <TableCell class="text-xs text-muted-foreground">{{
                new Date(fila.creado_en).toLocaleTimeString()
              }}</TableCell>
            </TableRow>
            <TableRow v-if="!cargando && filasTabla.length === 0">
              <TableCell colspan="5">
                <div class="flex flex-col items-center gap-2 py-8 text-muted-foreground">
                  <Inbox class="h-8 w-8" stroke-width="1.5" />
                  <span>Todavía no pasó ninguna caja</span>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  </div>
</template>
