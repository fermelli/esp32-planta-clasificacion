<script setup lang="ts">
import { ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card'
import { Boxes, User, Lock, LogIn, LoaderCircle } from 'lucide-vue-next'

const auth = useAuthStore()
const router = useRouter()
const route = useRoute()

const usuario = ref('')
const password = ref('')
const error = ref('')
const cargando = ref(false)

async function enviar() {
  error.value = ''
  cargando.value = true
  try {
    await auth.iniciarSesion(usuario.value, password.value)
    const next = typeof route.query.next === 'string' ? route.query.next : '/'
    router.push(next)
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'No se pudo iniciar sesión'
  } finally {
    cargando.value = false
  }
}
</script>

<template>
  <div class="relative flex min-h-screen items-center justify-center overflow-hidden px-4">
    <div class="pointer-events-none absolute inset-0 -z-10">
      <div
        class="absolute inset-0 opacity-[0.4] [background-image:radial-gradient(circle,var(--color-border)_1px,transparent_1px)] [background-size:24px_24px] [mask-image:radial-gradient(ellipse_60%_60%_at_50%_0%,black,transparent)]"
      />
      <div
        class="absolute -top-32 left-1/2 h-80 w-80 -translate-x-1/2 rounded-full bg-primary/25 blur-3xl"
      />
      <div class="absolute bottom-0 right-0 h-64 w-64 rounded-full bg-primary/10 blur-3xl" />
    </div>

    <div class="flex w-full max-w-sm flex-col items-center gap-6">
      <div class="flex items-center gap-2.5">
        <span
          class="flex h-10 w-10 items-center justify-center rounded-xl bg-primary text-primary-foreground shadow-md"
        >
          <Boxes class="h-5 w-5" stroke-width="1.8" />
        </span>
        <span class="text-lg font-semibold tracking-tight">Planta de clasificación</span>
      </div>

      <Card class="w-full shadow-xl">
        <CardHeader>
          <CardTitle class="text-lg text-foreground">Iniciar sesión</CardTitle>
          <CardDescription>Ingresá con tu usuario y contraseña del panel.</CardDescription>
        </CardHeader>
        <CardContent>
          <form class="flex flex-col gap-4" @submit.prevent="enviar">
            <div class="flex flex-col gap-1.5">
              <Label for="usuario">Usuario</Label>
              <div class="relative">
                <User class="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" stroke-width="1.8" />
                <Input id="usuario" v-model="usuario" class="pl-9" autocomplete="username" />
              </div>
            </div>
            <div class="flex flex-col gap-1.5">
              <Label for="password">Contraseña</Label>
              <div class="relative">
                <Lock class="pointer-events-none absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" stroke-width="1.8" />
                <Input
                  id="password"
                  v-model="password"
                  type="password"
                  class="pl-9"
                  autocomplete="current-password"
                />
              </div>
            </div>
            <p v-if="error" class="text-sm text-destructive">{{ error }}</p>
            <Button type="submit" class="gap-1.5" :disabled="cargando">
              <LoaderCircle v-if="cargando" class="h-4 w-4 animate-spin" />
              <LogIn v-else class="h-4 w-4" stroke-width="1.8" />
              {{ cargando ? 'Entrando...' : 'Entrar' }}
            </Button>
          </form>
        </CardContent>
      </Card>
    </div>
  </div>
</template>
