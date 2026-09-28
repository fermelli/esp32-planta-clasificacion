<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { API_URL, apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { CapturaColor, MetricasColor, ResumenColor } from '@/lib/types'
import PruebaCamaraCard from '@/components/PruebaCamaraCard.vue'
import { Card, CardHeader, CardTitle, CardContent, CardDescription } from '@/components/ui/card'
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
import { Palette, Camera, Brain, CircleCheck, CircleX, GitCompare } from 'lucide-vue-next'

const live = useLiveStore()

const COLORES: Record<string, string> = {
  rojo: '#ef4444',
  verde: '#059669',
  azul: '#3b82f6',
}

const resumen = ref<ResumenColor | null>(null)

const estado = computed(() => {
  const v = live.camaras?.color
  if (!v) return null
  if (v.activo) return { variante: 'success' as const, texto: 'activa: foto por cada caja' }
  if (v.flag) return { variante: 'destructive' as const, texto: 'sin cámara conectada' }
  if (v.online)
    return { variante: 'outline' as const, texto: 'cámara conectada, CAMARA_COLOR=false' }
  return { variante: 'outline' as const, texto: 'apagada (CAMARA_COLOR=false)' }
})
const capturas = ref<CapturaColor[]>([])
const cargando = ref(true)
const entrenando = ref(false)
const error = ref('')
const metricasNuevas = ref<MetricasColor | null>(null)

async function cargar() {
  const [r, c] = await Promise.all([
    apiFetch<ResumenColor>('/api/color/resumen'),
    apiFetch<CapturaColor[]>('/api/color/capturas?limite=20'),
  ])
  resumen.value = r
  capturas.value = c
}

onMounted(async () => {
  try {
    await cargar()
  } finally {
    cargando.value = false
  }
})

// Cada foto clasificada llega por WebSocket: recargar contadores y tabla.
watch(() => live.ultimasClasificaciones.length, cargar)

const metricas = computed(() => metricasNuevas.value ?? resumen.value?.metricas ?? null)
const ultima = computed(() => capturas.value[0] ?? null)
const totalDataset = computed(() =>
  Object.values(resumen.value?.dataset ?? {}).reduce((a, n) => a + n, 0),
)

async function entrenar() {
  error.value = ''
  entrenando.value = true
  try {
    metricasNuevas.value = await apiFetch<MetricasColor>('/api/color/entrenar', { method: 'POST' })
    await cargar()
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'No se pudo entrenar'
  } finally {
    entrenando.value = false
  }
}

function punto(color: string | null) {
  return { backgroundColor: (color && COLORES[color]) || '#999' }
}
</script>

<template>
  <div class="flex flex-col gap-6">
    <Card>
      <CardHeader>
        <CardTitle class="flex items-center justify-between">
          <span class="flex items-center gap-2">
            <Palette class="h-4 w-4 text-muted-foreground" stroke-width="2" />
            Cámara de color
          </span>
          <Badge v-if="estado" :variant="estado.variante">{{ estado.texto }}</Badge>
        </CardTitle>
        <CardDescription>
          Cada caja que detecta el sensor TCS3472 dispara una foto. Un modelo scikit-learn entrenado
          con esas mismas fotos (etiquetadas por el sensor) da una segunda opinión, y acá se
          comparan las dos.
        </CardDescription>
      </CardHeader>
    </Card>

    <PruebaCamaraCard version="color" />

    <div class="grid grid-cols-1 gap-4 sm:grid-cols-3">
      <Card>
        <CardHeader class="pb-1">
          <CardTitle class="flex items-center gap-2 text-sm font-medium text-muted-foreground">
            <GitCompare class="h-3.5 w-3.5" stroke-width="2" />
            Acuerdo sensor vs. IA
          </CardTitle>
        </CardHeader>
        <CardContent>
          <p class="text-4xl font-bold tabular-nums tracking-tight">
            {{ resumen?.acuerdo_pct == null ? '—' : `${resumen.acuerdo_pct}%` }}
          </p>
          <p class="text-xs text-muted-foreground">
            {{ resumen?.coinciden ?? 0 }} de {{ resumen?.con_ia ?? 0 }} cajas clasificadas
          </p>
        </CardContent>
      </Card>

      <Card>
        <CardHeader class="pb-1">
          <CardTitle class="flex items-center gap-2 text-sm font-medium text-muted-foreground">
            <Camera class="h-3.5 w-3.5" stroke-width="2" />
            Fotos en el dataset
          </CardTitle>
        </CardHeader>
        <CardContent>
          <p class="text-4xl font-bold tabular-nums tracking-tight">{{ totalDataset }}</p>
          <p class="flex flex-wrap gap-x-3 text-xs text-muted-foreground">
            <span
              v-for="(n, color) in resumen?.dataset ?? {}"
              :key="color"
              class="flex items-center gap-1"
            >
              <span class="h-2 w-2 rounded-full" :style="punto(String(color))" />
              {{ color }} {{ n }}
            </span>
            <span v-if="!totalDataset">todavía ninguna</span>
          </p>
        </CardContent>
      </Card>

      <Card>
        <CardHeader class="pb-1">
          <CardTitle class="flex items-center gap-2 text-sm font-medium text-muted-foreground">
            <Brain class="h-3.5 w-3.5" stroke-width="2" />
            Modelo
          </CardTitle>
        </CardHeader>
        <CardContent class="flex flex-col gap-2">
          <p class="text-4xl font-bold tabular-nums tracking-tight">
            {{ metricas ? `${(metricas.accuracy * 100).toFixed(0)}%` : '—' }}
          </p>
          <p class="text-xs text-muted-foreground">
            {{
              metricas
                ? `accuracy en ${metricas.n_prueba} fotos de prueba`
                : resumen?.modelo_entrenado
                  ? 'entrenado'
                  : 'sin entrenar'
            }}
          </p>
          <Button size="sm" class="w-fit gap-1.5" :disabled="entrenando" @click="entrenar">
            <Brain class="h-3.5 w-3.5" stroke-width="2" />
            {{ entrenando ? 'Entrenando...' : 'Entrenar con el dataset' }}
          </Button>
        </CardContent>
      </Card>
    </div>
    <p v-if="error" class="text-sm text-destructive">{{ error }}</p>
    <p v-else-if="resumen && !resumen.modelo_entrenado" class="text-sm text-muted-foreground">
      Hacen falta al menos {{ resumen.min_por_clase }} fotos de cada color (2 colores como mínimo)
      para entrenar. Mientras tanto el sistema solo junta el dataset.
    </p>

    <div class="grid grid-cols-1 gap-4 lg:grid-cols-2">
      <Card>
        <CardHeader>
          <CardTitle>Última caja</CardTitle>
        </CardHeader>
        <CardContent v-if="ultima" class="flex flex-col gap-3">
          <img
            :src="`${API_URL}/capturas/${ultima.imagen}`"
            alt="última captura"
            class="w-full rounded-lg border border-border object-cover"
          />
          <div class="flex flex-wrap items-center gap-2 text-sm">
            <Badge variant="outline" class="gap-1.5">
              <span class="h-2 w-2 rounded-full" :style="punto(ultima.color_sensor)" />
              sensor: {{ ultima.color_sensor }}
            </Badge>
            <Badge variant="outline" class="gap-1.5">
              <span class="h-2 w-2 rounded-full" :style="punto(ultima.color_ia)" />
              IA: {{ ultima.color_ia ?? 'sin modelo' }}
              <template v-if="ultima.confianza != null">
                ({{ (ultima.confianza * 100).toFixed(0) }}%)
              </template>
            </Badge>
            <Badge
              v-if="ultima.coincide !== null"
              :variant="ultima.coincide ? 'success' : 'destructive'"
            >
              {{ ultima.coincide ? 'coinciden' : 'difieren' }}
            </Badge>
          </div>
        </CardContent>
        <CardContent v-else class="py-8 text-center text-sm text-muted-foreground">
          Todavía no hay fotos
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Matriz de confusión</CardTitle>
          <CardDescription>
            Sobre las fotos de prueba (20 % del dataset, que el modelo no vio al entrenar). Filas:
            color real; columnas: lo que dijo el modelo.
          </CardDescription>
        </CardHeader>
        <CardContent v-if="metricas">
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>real \ IA</TableHead>
                <TableHead v-for="c in metricas.clases" :key="c" class="capitalize">{{
                  c
                }}</TableHead>
                <TableHead>Recall</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              <TableRow v-for="(fila, i) in metricas.matriz_confusion" :key="i">
                <TableCell class="capitalize">
                  <span class="flex items-center gap-2">
                    <span class="h-2 w-2 rounded-full" :style="punto(metricas.clases[i]!)" />
                    {{ metricas.clases[i] }}
                  </span>
                </TableCell>
                <TableCell
                  v-for="(n, j) in fila"
                  :key="j"
                  class="tabular-nums"
                  :class="i === j ? 'font-semibold text-foreground' : 'text-muted-foreground'"
                >
                  {{ n }}
                </TableCell>
                <TableCell class="tabular-nums text-muted-foreground">
                  {{ (metricas.reporte[metricas.clases[i]!]?.recall ?? 0).toFixed(2) }}
                </TableCell>
              </TableRow>
            </TableBody>
          </Table>
          <p class="mt-2 text-xs text-muted-foreground">
            Entrenado con {{ metricas.n_entrenamiento }} fotos, modelo final con
            {{ metricas.n_muestras }}. {{ new Date(metricas.entrenado_en).toLocaleString() }}
          </p>
        </CardContent>
        <CardContent v-else class="py-8 text-center text-sm text-muted-foreground">
          Se muestra después de entrenar
        </CardContent>
      </Card>
    </div>

    <Card>
      <CardHeader>
        <CardTitle>Capturas recientes</CardTitle>
      </CardHeader>
      <CardContent>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Foto</TableHead>
              <TableHead>Sensor</TableHead>
              <TableHead>IA</TableHead>
              <TableHead>Confianza</TableHead>
              <TableHead>Resultado</TableHead>
              <TableHead>Hora</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <template v-if="cargando">
              <TableRow v-for="i in 3" :key="i">
                <TableCell colspan="6"
                  ><div class="h-4 w-full animate-pulse rounded bg-muted"
                /></TableCell>
              </TableRow>
            </template>
            <TableRow v-for="c in capturas" :key="c.evento_id">
              <TableCell>
                <img
                  :src="`${API_URL}/capturas/${c.imagen}`"
                  alt="captura"
                  class="h-12 w-16 rounded object-cover"
                  loading="lazy"
                />
              </TableCell>
              <TableCell class="capitalize">
                <span class="flex items-center gap-2">
                  <span class="h-2 w-2 rounded-full" :style="punto(c.color_sensor)" />
                  {{ c.color_sensor }}
                </span>
              </TableCell>
              <TableCell class="capitalize">
                <span v-if="c.color_ia" class="flex items-center gap-2">
                  <span class="h-2 w-2 rounded-full" :style="punto(c.color_ia)" />
                  {{ c.color_ia }}
                </span>
                <span v-else class="text-muted-foreground">—</span>
              </TableCell>
              <TableCell class="tabular-nums text-muted-foreground">
                {{ c.confianza == null ? '—' : `${(c.confianza * 100).toFixed(0)}%` }}
              </TableCell>
              <TableCell>
                <Badge
                  v-if="c.coincide !== null"
                  :variant="c.coincide ? 'success' : 'destructive'"
                  class="gap-1"
                >
                  <CircleCheck v-if="c.coincide" class="h-3 w-3" stroke-width="2.5" />
                  <CircleX v-else class="h-3 w-3" stroke-width="2.5" />
                  {{ c.coincide ? 'coinciden' : 'difieren' }}
                </Badge>
              </TableCell>
              <TableCell class="text-xs text-muted-foreground">{{
                new Date(c.creado_en).toLocaleTimeString()
              }}</TableCell>
            </TableRow>
            <TableRow v-if="!cargando && capturas.length === 0">
              <TableCell colspan="6">
                <div class="flex flex-col items-center gap-2 py-8 text-muted-foreground">
                  <Camera class="h-8 w-8" stroke-width="1.5" />
                  <span>Todavía no hay fotos: activá CAMARA_COLOR y pasá una caja</span>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  </div>
</template>
