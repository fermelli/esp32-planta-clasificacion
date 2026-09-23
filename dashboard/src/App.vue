<script setup lang="ts">
import { watch } from 'vue'
import { useRoute, useRouter, RouterLink, RouterView } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useLiveStore } from '@/stores/live'
import { useThemeStore } from '@/stores/theme'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import {
  Boxes,
  Gauge,
  History,
  KeyRound,
  BellRing,
  Sun,
  Moon,
  LogOut,
} from 'lucide-vue-next'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()
const live = useLiveStore()
const theme = useThemeStore()

function salir() {
  auth.cerrarSesion()
  router.push('/login')
}

watch(
  () => auth.token,
  (token) => {
    if (token) live.conectar()
  },
  { immediate: true },
)

const NAV = [
  { to: '/', label: 'Producción', icon: Gauge },
  { to: '/historico', label: 'Histórico', icon: History },
  { to: '/intentos', label: 'Accesos', icon: KeyRound },
  { to: '/alertas', label: 'Alertas', icon: BellRing },
]

function iniciales(nombre: string | null): string {
  if (!nombre) return '?'
  return nombre
    .trim()
    .split(/\s+/)
    .slice(0, 2)
    .map((p) => p[0]?.toUpperCase())
    .join('')
}
</script>

<template>
  <div class="min-h-screen bg-background">
    <header v-if="!route.meta.publica" class="glass sticky top-0 z-20 border-b border-border">
      <div class="mx-auto flex max-w-6xl items-center gap-3 px-4 py-3 sm:gap-6">
        <div class="flex shrink-0 items-center gap-2">
          <span
            class="flex h-8 w-8 items-center justify-center rounded-lg bg-primary text-primary-foreground shadow-sm"
          >
            <Boxes class="h-4.5 w-4.5" stroke-width="1.8" />
          </span>
          <span class="hidden font-semibold tracking-tight sm:inline">Planta de clasificación</span>
        </div>

        <nav
          class="flex min-w-0 flex-1 gap-1 overflow-x-auto [scrollbar-width:none] [&::-webkit-scrollbar]:hidden"
        >
          <RouterLink
            v-for="item in NAV"
            :key="item.to"
            :to="item.to"
            class="flex shrink-0 items-center gap-1.5 rounded-md px-3 py-1.5 text-sm font-medium whitespace-nowrap text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
            active-class="!bg-primary !text-primary-foreground shadow-sm"
          >
            <component :is="item.icon" class="h-3.5 w-3.5" stroke-width="2" />
            {{ item.label }}
          </RouterLink>
        </nav>

        <div class="ml-auto flex shrink-0 items-center gap-2 sm:gap-3">
          <Badge :variant="live.conectado ? 'success' : 'destructive'" class="gap-1.5">
            <span
              class="h-1.5 w-1.5 rounded-full bg-primary-foreground"
              :class="live.conectado && 'animate-pulse'"
            />
            <span class="hidden sm:inline">{{ live.conectado ? 'en vivo' : 'desconectado' }}</span>
          </Badge>

          <button
            class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
            :aria-label="theme.oscuro ? 'Cambiar a modo claro' : 'Cambiar a modo oscuro'"
            @click="theme.alternar"
          >
            <Sun v-if="theme.oscuro" class="h-4 w-4" stroke-width="2" />
            <Moon v-else class="h-4 w-4" stroke-width="2" />
          </button>

          <div class="hidden items-center gap-2 sm:flex">
            <span
              class="flex h-7 w-7 items-center justify-center rounded-full bg-accent text-xs font-semibold text-accent-foreground"
            >
              {{ iniciales(auth.nombre) }}
            </span>
            <span class="text-sm text-muted-foreground">{{ auth.nombre }}</span>
          </div>

          <Button variant="outline" size="sm" class="gap-1.5" @click="salir">
            <LogOut class="h-3.5 w-3.5" stroke-width="2" />
            <span class="hidden sm:inline">Salir</span>
          </Button>
        </div>
      </div>
    </header>

    <main class="mx-auto max-w-6xl px-4 py-6">
      <RouterView v-slot="{ Component }">
        <transition name="fade" mode="out-in">
          <component :is="Component" />
        </transition>
      </RouterView>
    </main>
  </div>
</template>

<style scoped>
.fade-enter-active,
.fade-leave-active {
  transition:
    opacity 0.15s ease,
    transform 0.15s ease;
}
.fade-enter-from {
  opacity: 0;
  transform: translateY(4px);
}
.fade-leave-to {
  opacity: 0;
}
</style>
