<script setup lang="ts">
import { ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card'

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
  <div class="flex min-h-screen items-center justify-center bg-muted/30 px-4">
    <Card class="w-full max-w-sm">
      <CardHeader>
        <CardTitle class="text-lg text-foreground">Planta de clasificación</CardTitle>
        <CardDescription>Ingresá con tu usuario y contraseña del panel.</CardDescription>
      </CardHeader>
      <CardContent>
        <form class="flex flex-col gap-4" @submit.prevent="enviar">
          <div class="flex flex-col gap-1.5">
            <Label for="usuario">Usuario</Label>
            <Input id="usuario" v-model="usuario" autocomplete="username" />
          </div>
          <div class="flex flex-col gap-1.5">
            <Label for="password">Contraseña</Label>
            <Input id="password" v-model="password" type="password" autocomplete="current-password" />
          </div>
          <p v-if="error" class="text-sm text-destructive">{{ error }}</p>
          <Button type="submit" :disabled="cargando">
            {{ cargando ? 'Entrando...' : 'Entrar' }}
          </Button>
        </form>
      </CardContent>
    </Card>
  </div>
</template>
