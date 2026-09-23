<script setup lang="ts">
import { watch } from 'vue'
import { useRoute, useRouter, RouterLink, RouterView } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useLiveStore } from '@/stores/live'
import { useThemeStore } from '@/stores/theme'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'

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
  { to: '/', label: 'Producción' },
  { to: '/historico', label: 'Histórico' },
  { to: '/intentos', label: 'Accesos' },
  { to: '/alertas', label: 'Alertas' },
]
</script>

<template>
  <div class="min-h-screen bg-background">
    <header v-if="!route.meta.publica" class="glass sticky top-0 z-20 border-b border-border">
      <div class="mx-auto flex max-w-6xl items-center gap-3 px-4 py-3 sm:gap-6">
        <div class="flex shrink-0 items-center gap-2">
          <span
            class="flex h-8 w-8 items-center justify-center rounded-lg bg-primary text-primary-foreground shadow-sm"
          >
            <svg
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="1.8"
              stroke-linecap="round"
              stroke-linejoin="round"
              class="h-4.5 w-4.5"
            >
              <path d="M21 7.5 12 3 3 7.5l9 4.5 9-4.5Z" />
              <path d="M3 7.5v9l9 4.5 9-4.5v-9" />
              <path d="M12 12v9" />
            </svg>
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
            class="shrink-0 rounded-md px-3 py-1.5 text-sm font-medium whitespace-nowrap text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
            active-class="!bg-primary !text-primary-foreground shadow-sm"
          >
            {{ item.label }}
          </RouterLink>
        </nav>

        <div class="ml-auto flex shrink-0 items-center gap-2 sm:gap-3">
          <Badge :variant="live.conectado ? 'success' : 'destructive'" class="gap-1.5">
            <span
              class="h-1.5 w-1.5 rounded-full"
              :class="
                live.conectado ? 'bg-primary-foreground animate-pulse' : 'bg-primary-foreground'
              "
            />
            {{ live.conectado ? 'en vivo' : 'desconectado' }}
          </Badge>

          <button
            class="flex h-8 w-8 items-center justify-center rounded-md text-muted-foreground transition-colors hover:bg-accent hover:text-accent-foreground"
            :aria-label="theme.oscuro ? 'Cambiar a modo claro' : 'Cambiar a modo oscuro'"
            @click="theme.alternar"
          >
            <svg
              v-if="theme.oscuro"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              class="h-4 w-4"
            >
              <circle cx="12" cy="12" r="4" />
              <path
                d="M12 2v2M12 20v2M4 12H2M22 12h-2M4.93 4.93l1.41 1.41M17.66 17.66l1.41 1.41M4.93 19.07l1.41-1.41M17.66 6.34l1.41-1.41"
              />
            </svg>
            <svg
              v-else
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              class="h-4 w-4"
            >
              <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79Z" />
            </svg>
          </button>

          <span class="hidden text-sm text-muted-foreground sm:inline">{{ auth.nombre }}</span>
          <Button variant="outline" size="sm" @click="salir"> Salir </Button>
        </div>
      </div>
    </header>

    <main class="mx-auto max-w-6xl px-4 py-6">
      <RouterView />
    </main>
  </div>
</template>
