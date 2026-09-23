<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { Alerta } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import { BellRing, PackageCheck, ShieldAlert, Info, BellOff } from 'lucide-vue-next'

const live = useLiveStore()
const iniciales = ref<Alerta[]>([])
const cargando = ref(true)

onMounted(async () => {
  try {
    iniciales.value = await apiFetch<Alerta[]>('/api/alertas?limite=30')
  } finally {
    cargando.value = false
  }
})

const TIPO_INFO: Record<string, { variant: 'success' | 'destructive' | 'warning'; icon: typeof PackageCheck }> = {
  lote_completo: { variant: 'success', icon: PackageCheck },
  login_bloqueado: { variant: 'destructive', icon: ShieldAlert },
}

function infoTipo(tipo: string) {
  return TIPO_INFO[tipo] ?? { variant: 'warning' as const, icon: Info }
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
      <CardTitle class="flex items-center gap-2">
        <BellRing class="h-4 w-4 text-muted-foreground" stroke-width="2" />
        Alertas
      </CardTitle>
    </CardHeader>
    <CardContent>
      <ul v-if="cargando" class="flex flex-col gap-2">
        <li v-for="i in 3" :key="i" class="h-12 animate-pulse rounded-md bg-muted" />
      </ul>
      <ul v-else class="flex flex-col gap-2">
        <li
          v-for="(a, i) in alertas"
          :key="i"
          class="flex items-center gap-3 rounded-md border border-border p-3 text-sm transition-colors hover:bg-muted/50"
        >
          <Badge :variant="infoTipo(a.tipo).variant" class="shrink-0 gap-1">
            <component :is="infoTipo(a.tipo).icon" class="h-3 w-3" stroke-width="2.5" />
            {{ a.tipo }}
          </Badge>
          <span class="flex-1">{{ a.mensaje }}</span>
          <span class="shrink-0 text-xs text-muted-foreground">{{
            new Date(a.creado_en).toLocaleString()
          }}</span>
        </li>
        <li v-if="alertas.length === 0" class="flex flex-col items-center gap-2 py-8 text-muted-foreground">
          <BellOff class="h-8 w-8" stroke-width="1.5" />
          <span>Sin alertas</span>
        </li>
      </ul>
    </CardContent>
  </Card>
</template>
