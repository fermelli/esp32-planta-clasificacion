<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { ConteoColor, EventoCaja } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import { Table, TableHeader, TableBody, TableRow, TableHead, TableCell } from '@/components/ui/table'
import ContadorBinario from '@/components/ContadorBinario.vue'

const live = useLiveStore()

const COLORES = [
  { id: 'rojo', label: 'Rojo', hex: '#ef4444' },
  { id: 'verde', label: 'Verde', hex: '#22c55e' },
  { id: 'azul', label: 'Azul', hex: '#3b82f6' },
] as const

const conteos = ref<Record<string, ConteoColor>>({})
const eventosIniciales = ref<EventoCaja[]>([])
const enviandoComando = ref<string | null>(null)

async function cargar() {
  const [c, e] = await Promise.all([
    apiFetch<ConteoColor[]>('/api/conteos'),
    apiFetch<EventoCaja[]>('/api/eventos?limite=15'),
  ])
  conteos.value = Object.fromEntries(c.map((x) => [x.color, x]))
  eventosIniciales.value = e
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
      <Card v-for="color in COLORES" :key="color.id">
        <CardHeader class="pb-2">
          <CardTitle>{{ color.label }}</CardTitle>
        </CardHeader>
        <CardContent class="flex items-center justify-between">
          <ContadorBinario
            :valor="conteos[color.id]?.conteo_actual ?? 0"
            :color-encendido="color.hex"
            label="LEDs"
          />
          <div class="text-right text-sm text-muted-foreground">
            <p>Total: <span class="font-medium text-foreground">{{ conteos[color.id]?.total_historico ?? 0 }}</span></p>
            <p>Lotes: <span class="font-medium text-foreground">{{ conteos[color.id]?.lotes_completados ?? 0 }}</span></p>
          </div>
        </CardContent>
      </Card>
    </div>

    <Card>
      <CardHeader>
        <CardTitle>Cinta y puerta</CardTitle>
      </CardHeader>
      <CardContent class="flex flex-wrap items-center gap-3">
        <Badge variant="outline">
          Cinta: {{ live.sorterEstado.cinta_estado ?? 'sin datos' }}
        </Badge>
        <Badge variant="outline">
          Puerta: {{ live.sorterEstado.puerta_abierta === undefined ? 'sin datos' : (live.sorterEstado.puerta_abierta ? 'abierta' : 'cerrada') }}
        </Badge>
        <div class="ml-auto flex flex-wrap gap-2">
          <Button size="sm" variant="secondary" :disabled="!!enviandoComando" @click="enviarComando('motor', 0)">Apagar cinta</Button>
          <Button size="sm" variant="secondary" :disabled="!!enviandoComando" @click="enviarComando('motor', 1)">Cinta low</Button>
          <Button size="sm" variant="secondary" :disabled="!!enviandoComando" @click="enviarComando('motor', 2)">Cinta full</Button>
          <Button size="sm" variant="outline" :disabled="!!enviandoComando" @click="enviarComando('puerta', 1)">Abrir puerta</Button>
          <Button size="sm" variant="outline" :disabled="!!enviandoComando" @click="enviarComando('puerta', 0)">Cerrar puerta</Button>
          <Button size="sm" variant="destructive" :disabled="!!enviandoComando" @click="enviarComando('reset_counts')">Reset contadores</Button>
        </div>
      </CardContent>
    </Card>

    <Card>
      <CardHeader>
        <CardTitle>Últimas cajas</CardTitle>
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
              <TableCell class="capitalize">{{ fila.color }}</TableCell>
              <TableCell class="tabular-nums">{{ fila.conteo }}</TableCell>
              <TableCell>
                <Badge v-if="fila.lote_completo" variant="success">completo</Badge>
              </TableCell>
              <TableCell class="text-xs text-muted-foreground tabular-nums">{{ fila.r }}/{{ fila.g }}/{{ fila.b }} · {{ fila.c }}</TableCell>
              <TableCell class="text-xs text-muted-foreground">{{ new Date(fila.creado_en).toLocaleTimeString() }}</TableCell>
            </TableRow>
            <TableRow v-if="filasTabla.length === 0">
              <TableCell colspan="5" class="text-center text-muted-foreground">Todavía no pasó ninguna caja</TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  </div>
</template>
