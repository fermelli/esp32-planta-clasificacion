<script setup lang="ts">
import { watch } from 'vue'
import { useRoute, RouterLink, RouterView } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useLiveStore } from '@/stores/live'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'

const route = useRoute()
const auth = useAuthStore()
const live = useLiveStore()

watch(
  () => auth.token,
  (token) => {
    if (token) live.conectar()
  },
  { immediate: true },
)

const NAV = [
  { to: '/', label: 'Producción' },
  { to: '/historico', label: 'Histórico' },
  { to: '/intentos', label: 'Accesos' },
  { to: '/alertas', label: 'Alertas' },
]
</script>

<template>
  <div class="min-h-screen bg-background">
    <header v-if="!route.meta.publica" class="border-b border-border">
      <div class="mx-auto flex max-w-6xl items-center gap-6 px-4 py-3">
        <span class="font-semibold">Planta de clasificación</span>
        <nav class="flex gap-1">
          <RouterLink
            v-for="item in NAV"
            :key="item.to"
            :to="item.to"
            class="rounded-md px-3 py-1.5 text-sm text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
            active-class="bg-accent text-accent-foreground"
          >
            {{ item.label }}
          </RouterLink>
        </nav>
        <div class="ml-auto flex items-center gap-3">
          <Badge :variant="live.conectado ? 'success' : 'destructive'">
            {{ live.conectado ? 'en vivo' : 'desconectado' }}
          </Badge>
          <span class="text-sm text-muted-foreground">{{ auth.nombre }}</span>
          <Button variant="outline" size="sm" @click="auth.cerrarSesion(); $router.push('/login')">
            Salir
          </Button>
        </div>
      </div>
    </header>

    <main class="mx-auto max-w-6xl px-4 py-6">
      <RouterView />
    </main>
  </div>
</template>
