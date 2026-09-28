<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { API_URL, apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { RostroConfig, RostroUsuario, VerificacionRostro } from '@/lib/types'
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
import {
  ScanFace,
  UserPlus,
  Trash2,
  CircleCheck,
  CircleX,
  Camera,
  ShieldCheck,
} from 'lucide-vue-next'

const live = useLiveStore()

const config = ref<RostroConfig | null>(null)
const usuarios = ref<RostroUsuario[]>([])
const verificaciones = ref<VerificacionRostro[]>([])
const cargando = ref(true)
const enrolando = ref<number | null>(null)

const camaraOnline = computed(() => live.camaras?.rostro.online ?? false)
const estado = computed(() => {
  const v = live.camaras?.rostro
  if (!v) return null
  if (v.activo) return { variante: 'success' as const, texto: 'PIN + rostro activo' }
  if (v.flag) return { variante: 'destructive' as const, texto: 'sin cámara: entra solo con PIN' }
  return { variante: 'outline' as const, texto: 'apagado (solo PIN)' }
})
const error = ref('')

async function cargarUsuarios() {
  usuarios.value = await apiFetch<RostroUsuario[]>('/api/rostro/usuarios')
}

async function cargarVerificaciones() {
  verificaciones.value = await apiFetch<VerificacionRostro[]>(
    '/api/rostro/verificaciones?limite=20',
  )
}

onMounted(async () => {
  try {
    const [c] = await Promise.all([
      apiFetch<RostroConfig>('/api/rostro/config'),
      cargarUsuarios(),
      cargarVerificaciones(),
    ])
    config.value = c
  } finally {
    cargando.value = false
  }
})

// Cada foto verificada durante un login llega por WebSocket: recargar la tabla.
watch(() => live.ultimasVerificaciones.length, cargarVerificaciones)

// La placa toma 5 fotos (~8 s) tras la orden: refrescar el contador mientras tanto.
let sondeo: ReturnType<typeof setInterval> | undefined
onBeforeUnmount(() => clearInterval(sondeo))

async function enrolar(u: RostroUsuario) {
  error.value = ''
  enrolando.value = u.usuario_id
  try {
    await apiFetch(`/api/rostro/enrolar/${u.usuario_id}`, { method: 'POST' })
    clearInterval(sondeo)
    let vueltas = 0
    sondeo = setInterval(async () => {
      await cargarUsuarios()
      if (++vueltas >= 8) {
        clearInterval(sondeo)
        enrolando.value = null
      }
    }, 2000)
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'No se pudo enviar la orden'
    enrolando.value = null
  }
}

async function borrar(u: RostroUsuario) {
  if (!confirm(`¿Borrar las ${u.muestras} muestras de rostro de ${u.nombre}?`)) return
  await apiFetch(`/api/rostro/muestras/${u.usuario_id}`, { method: 'DELETE' })
  await cargarUsuarios()
}
</script>

<template>
  <div class="flex flex-col gap-6">
    <Card>
      <CardHeader>
        <CardTitle class="flex items-center justify-between">
          <span class="flex items-center gap-2">
            <ShieldCheck class="h-4 w-4 text-muted-foreground" stroke-width="2" />
            Login por rostro
          </span>
          <Badge v-if="estado" :variant="estado.variante">{{ estado.texto }}</Badge>
        </CardTitle>
        <CardDescription>
          Con el login por rostro activo, el teclado pide el PIN y después verifica la cara con la
          cámara. Enrolá las caras antes de activarlo (variable LOGIN_ROSTRO en el servidor).
        </CardDescription>
      </CardHeader>
      <CardContent v-if="config" class="flex flex-wrap gap-4 text-sm text-muted-foreground">
        <span
          >Umbral de similitud: <b class="text-foreground">{{ config.umbral }}</b></span
        >
        <span>
          Cámara:
          <b :class="camaraOnline ? 'text-foreground' : 'text-destructive'">{{
            camaraOnline ? 'conectada' : 'desconectada'
          }}</b>
        </span>
        <span>
          Modelos:
          <b :class="config.modelos_cargados ? 'text-foreground' : 'text-destructive'">{{
            config.modelos_cargados ? 'cargados' : 'faltan (ml/descargar_modelos.py)'
          }}</b>
        </span>
      </CardContent>
    </Card>

    <PruebaCamaraCard version="rostro" />

    <div class="grid grid-cols-1 gap-4 sm:grid-cols-2">
      <Card v-for="u in usuarios" :key="u.usuario_id">
        <CardHeader class="pb-2">
          <CardTitle class="flex items-center justify-between text-base">
            <span class="flex items-center gap-2">
              <ScanFace class="h-4 w-4 text-muted-foreground" stroke-width="2" />
              {{ u.nombre }}
            </span>
            <Badge :variant="u.muestras > 0 ? 'success' : 'outline'">
              {{ u.muestras }} muestra{{ u.muestras === 1 ? '' : 's' }}
            </Badge>
          </CardTitle>
        </CardHeader>
        <CardContent class="flex flex-wrap items-center gap-2">
          <Button
            size="sm"
            class="gap-1.5"
            :disabled="enrolando !== null || !camaraOnline"
            :title="camaraOnline ? '' : 'La cámara de rostro no está conectada'"
            @click="enrolar(u)"
          >
            <UserPlus class="h-3.5 w-3.5" stroke-width="2" />
            {{ enrolando === u.usuario_id ? 'Mirá la cámara...' : 'Enrolar rostro' }}
          </Button>
          <Button
            size="sm"
            variant="outline"
            class="gap-1.5"
            :disabled="u.muestras === 0"
            @click="borrar(u)"
          >
            <Trash2 class="h-3.5 w-3.5" stroke-width="2" />
            Borrar
          </Button>
        </CardContent>
      </Card>
    </div>
    <p v-if="error" class="text-sm text-destructive">{{ error }}</p>
    <p v-if="enrolando !== null" class="text-sm text-muted-foreground">
      La cámara toma 5 fotos, una cada ~1 s. Mirá al lente de frente y con buena luz.
    </p>

    <Card>
      <CardHeader>
        <CardTitle class="flex items-center gap-2">
          <Camera class="h-4 w-4 text-muted-foreground" stroke-width="2" />
          Verificaciones recientes
        </CardTitle>
      </CardHeader>
      <CardContent>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Foto</TableHead>
              <TableHead>Usuario</TableHead>
              <TableHead>Similitud</TableHead>
              <TableHead>Resultado</TableHead>
              <TableHead>Hora</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <template v-if="cargando">
              <TableRow v-for="i in 3" :key="i">
                <TableCell colspan="5"
                  ><div class="h-4 w-full animate-pulse rounded bg-muted"
                /></TableCell>
              </TableRow>
            </template>
            <TableRow v-for="v in verificaciones" :key="v.id">
              <TableCell>
                <img
                  v-if="v.imagen"
                  :src="`${API_URL}/capturas/${v.imagen}`"
                  alt="captura"
                  class="h-12 w-16 rounded object-cover"
                  loading="lazy"
                />
              </TableCell>
              <TableCell>{{ v.usuario_nombre }}</TableCell>
              <TableCell class="tabular-nums">
                {{ v.similitud === null ? '—' : v.similitud.toFixed(3) }}
                <span v-if="config" class="text-xs text-muted-foreground">
                  / {{ config.umbral }}
                </span>
              </TableCell>
              <TableCell>
                <Badge :variant="v.exito ? 'success' : 'destructive'" class="gap-1">
                  <CircleCheck v-if="v.exito" class="h-3 w-3" stroke-width="2.5" />
                  <CircleX v-else class="h-3 w-3" stroke-width="2.5" />
                  {{ v.exito ? 'coincide' : 'no coincide' }}
                </Badge>
              </TableCell>
              <TableCell class="text-xs text-muted-foreground">{{
                new Date(v.creado_en).toLocaleString()
              }}</TableCell>
            </TableRow>
            <TableRow v-if="!cargando && verificaciones.length === 0">
              <TableCell colspan="5">
                <div class="flex flex-col items-center gap-2 py-8 text-muted-foreground">
                  <ScanFace class="h-8 w-8" stroke-width="1.5" />
                  <span>Todavía no hubo verificaciones</span>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  </div>
</template>
