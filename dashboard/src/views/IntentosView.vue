<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { IntentoLogin } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import {
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
} from '@/components/ui/table'
import { KeyRound, CircleCheck, CircleX, ShieldOff } from 'lucide-vue-next'

const live = useLiveStore()
const iniciales = ref<IntentoLogin[]>([])
const cargando = ref(true)

onMounted(async () => {
  try {
    iniciales.value = await apiFetch<IntentoLogin[]>('/api/intentos?limite=30')
  } finally {
    cargando.value = false
  }
})

const filas = computed(() => {
  const vivos = live.ultimosIntentos.map((i) => ({
    usuario_nombre: i.nombre,
    exito: i.exito,
    origen: 'keypad' as const,
    creado_en: new Date().toISOString(),
  }))
  return [...vivos, ...iniciales.value].slice(0, 30)
})
</script>

<template>
  <Card>
    <CardHeader>
      <CardTitle class="flex items-center gap-2">
        <KeyRound class="h-4 w-4 text-muted-foreground" stroke-width="2" />
        Intentos de acceso
      </CardTitle>
    </CardHeader>
    <CardContent>
      <Table>
        <TableHeader>
          <TableRow>
            <TableHead>Usuario</TableHead>
            <TableHead>Resultado</TableHead>
            <TableHead>Origen</TableHead>
            <TableHead>Hora</TableHead>
          </TableRow>
        </TableHeader>
        <TableBody>
          <template v-if="cargando">
            <TableRow v-for="i in 4" :key="i">
              <TableCell colspan="4"><div class="h-4 w-full animate-pulse rounded bg-muted" /></TableCell>
            </TableRow>
          </template>
          <TableRow v-for="(fila, i) in filas" :key="i">
            <TableCell>{{ fila.usuario_nombre ?? '—' }}</TableCell>
            <TableCell>
              <Badge :variant="fila.exito ? 'success' : 'destructive'" class="gap-1">
                <CircleCheck v-if="fila.exito" class="h-3 w-3" stroke-width="2.5" />
                <CircleX v-else class="h-3 w-3" stroke-width="2.5" />
                {{ fila.exito ? 'exitoso' : 'fallido' }}
              </Badge>
            </TableCell>
            <TableCell class="capitalize">{{ fila.origen }}</TableCell>
            <TableCell class="text-xs text-muted-foreground">{{
              new Date(fila.creado_en).toLocaleString()
            }}</TableCell>
          </TableRow>
          <TableRow v-if="!cargando && filas.length === 0">
            <TableCell colspan="4">
              <div class="flex flex-col items-center gap-2 py-8 text-muted-foreground">
                <ShieldOff class="h-8 w-8" stroke-width="1.5" />
                <span>Sin intentos registrados</span>
              </div>
            </TableCell>
          </TableRow>
        </TableBody>
      </Table>
    </CardContent>
  </Card>
</template>
