<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { apiFetch, ApiError } from '@/lib/api'
import type { Usuario } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import {
  Table,
  TableHeader,
  TableBody,
  TableRow,
  TableHead,
  TableCell,
} from '@/components/ui/table'
import { Users, UserPlus, LoaderCircle, CircleCheck } from 'lucide-vue-next'

const usuarios = ref<Usuario[]>([])
const cargando = ref(true)

const nombre = ref('')
const password = ref('')
const guardando = ref(false)
const error = ref('')
const exito = ref('')

async function cargarUsuarios() {
  usuarios.value = await apiFetch<Usuario[]>('/api/usuarios')
}

onMounted(async () => {
  try {
    await cargarUsuarios()
  } finally {
    cargando.value = false
  }
})

async function registrar() {
  error.value = ''
  exito.value = ''
  guardando.value = true
  try {
    const creado = await apiFetch<Usuario>('/api/usuarios', {
      method: 'POST',
      body: JSON.stringify({ nombre: nombre.value, password: password.value }),
    })
    usuarios.value = [...usuarios.value, creado]
    exito.value = `Operador "${creado.nombre}" registrado.`
    nombre.value = ''
    password.value = ''
  } catch (e) {
    error.value = e instanceof ApiError ? e.message : 'No se pudo registrar el operador'
  } finally {
    guardando.value = false
  }
}
</script>

<template>
  <div class="flex flex-col gap-6">
    <Card>
      <CardHeader>
        <CardTitle class="flex items-center gap-2">
          <UserPlus class="h-4 w-4 text-muted-foreground" stroke-width="2" />
          Registrar operador
        </CardTitle>
        <CardDescription>
          El PIN es la contraseña única del operador: sirve igual para el teclado 4x4 del ESP32 y
          para este panel — solo dígitos, hasta 8.
        </CardDescription>
      </CardHeader>
      <CardContent>
        <form class="flex flex-col gap-4 sm:max-w-sm" @submit.prevent="registrar">
          <div class="flex flex-col gap-1.5">
            <Label for="nombre">Nombre</Label>
            <Input id="nombre" v-model="nombre" autocomplete="off" required />
          </div>
          <div class="flex flex-col gap-1.5">
            <Label for="pin">PIN</Label>
            <Input
              id="pin"
              v-model="password"
              inputmode="numeric"
              pattern="\d{1,8}"
              maxlength="8"
              autocomplete="off"
              required
            />
          </div>
          <p v-if="error" class="text-sm text-destructive">{{ error }}</p>
          <p v-if="exito" class="flex items-center gap-1.5 text-sm text-emerald-600">
            <CircleCheck class="h-3.5 w-3.5" stroke-width="2.5" />
            {{ exito }}
          </p>
          <Button type="submit" class="gap-1.5 self-start" :disabled="guardando">
            <LoaderCircle v-if="guardando" class="h-4 w-4 animate-spin" />
            <UserPlus v-else class="h-4 w-4" stroke-width="1.8" />
            {{ guardando ? 'Registrando...' : 'Registrar' }}
          </Button>
        </form>
      </CardContent>
    </Card>

    <Card>
      <CardHeader>
        <CardTitle class="flex items-center gap-2">
          <Users class="h-4 w-4 text-muted-foreground" stroke-width="2" />
          Operadores registrados
        </CardTitle>
      </CardHeader>
      <CardContent>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Nombre</TableHead>
              <TableHead>Registrado</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <template v-if="cargando">
              <TableRow v-for="i in 3" :key="i">
                <TableCell colspan="2"
                  ><div class="h-4 w-full animate-pulse rounded bg-muted"
                /></TableCell>
              </TableRow>
            </template>
            <TableRow v-for="u in usuarios" :key="u.id">
              <TableCell>{{ u.nombre }}</TableCell>
              <TableCell class="text-xs text-muted-foreground">{{
                new Date(u.creado_en).toLocaleString()
              }}</TableCell>
            </TableRow>
            <TableRow v-if="!cargando && usuarios.length === 0">
              <TableCell colspan="2">
                <div class="flex flex-col items-center gap-2 py-8 text-muted-foreground">
                  <Users class="h-8 w-8" stroke-width="1.5" />
                  <span>Todavía no hay operadores</span>
                </div>
              </TableCell>
            </TableRow>
          </TableBody>
        </Table>
      </CardContent>
    </Card>
  </div>
</template>
