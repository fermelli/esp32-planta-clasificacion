<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { Alerta } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'

const live = useLiveStore()
const iniciales = ref<Alerta[]>([])

onMounted(async () => {
  iniciales.value = await apiFetch<Alerta[]>('/api/alertas?limite=30')
})

const TIPO_VARIANT: Record<string, 'success' | 'destructive' | 'warning'> = {
  lote_completo: 'success',
  login_bloqueado: 'destructive',
}

const alertas = computed(() => {
  const vivas = live.ultimasAlertas.map((a) => ({
    tipo: a.tipo,
    mensaje: a.mensaje,
    creado_en: new Date().toISOString(),
  }))
  return [...vivas, ...iniciales.value].slice(0, 30)
})
</script>

<template>
  <Card>
    <CardHeader>
      <CardTitle class="text-foreground">Alertas</CardTitle>
    </CardHeader>
    <CardContent>
      <ul class="flex flex-col gap-2">
        <li
          v-for="(a, i) in alertas"
          :key="i"
          class="flex items-center gap-3 rounded-md border border-border p-3 text-sm transition-colors hover:bg-muted/50"
        >
          <Badge :variant="TIPO_VARIANT[a.tipo] ?? 'outline'">{{ a.tipo }}</Badge>
          <span class="flex-1">{{ a.mensaje }}</span>
          <span class="text-xs text-muted-foreground">{{
            new Date(a.creado_en).toLocaleString()
          }}</span>
        </li>
        <li v-if="alertas.length === 0" class="py-6 text-center text-muted-foreground">
          Sin alertas
        </li>
      </ul>
    </CardContent>
  </Card>
</template>
