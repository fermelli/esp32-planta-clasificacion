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

const live = useLiveStore()
const iniciales = ref<IntentoLogin[]>([])

onMounted(async () => {
  iniciales.value = await apiFetch<IntentoLogin[]>('/api/intentos?limite=30')
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
      <CardTitle class="text-foreground">Intentos de acceso</CardTitle>
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
          <TableRow v-for="(fila, i) in filas" :key="i">
            <TableCell>{{ fila.usuario_nombre ?? '—' }}</TableCell>
            <TableCell>
              <Badge :variant="fila.exito ? 'success' : 'destructive'">
                {{ fila.exito ? 'exitoso' : 'fallido' }}
              </Badge>
            </TableCell>
            <TableCell class="capitalize">{{ fila.origen }}</TableCell>
            <TableCell class="text-xs text-muted-foreground">{{
              new Date(fila.creado_en).toLocaleString()
            }}</TableCell>
          </TableRow>
          <TableRow v-if="filas.length === 0">
            <TableCell colspan="4" class="text-center text-muted-foreground"
              >Sin intentos registrados</TableCell
            >
          </TableRow>
        </TableBody>
      </Table>
    </CardContent>
  </Card>
</template>
