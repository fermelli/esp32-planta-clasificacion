import { defineStore } from 'pinia'
import { ref } from 'vue'
import type { LoginResponse } from '@/lib/types'

function leerStorage(clave: string): string | null {
  try {
    return localStorage.getItem(clave)
  } catch {
    return null
  }
}

export const useAuthStore = defineStore('auth', () => {
  const token = ref<string | null>(leerStorage('planta_token'))
  const nombre = ref<string | null>(leerStorage('planta_nombre'))

  async function iniciarSesion(usuario: string, password: string) {
    const API_URL = import.meta.env.VITE_API_URL ?? 'http://localhost:8000'
    const res = await fetch(`${API_URL}/api/auth/login`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ nombre: usuario, password }),
    })
    if (!res.ok) {
      const detalle = await res.json().catch(() => ({}))
      throw new Error(detalle.detail ?? 'Usuario o contraseña incorrectos')
    }
    const data: LoginResponse = await res.json()
    token.value = data.access_token
    nombre.value = data.nombre
    try {
      localStorage.setItem('planta_token', data.access_token)
      localStorage.setItem('planta_nombre', data.nombre)
    } catch {
      /* el navegador puede bloquear localStorage (modo privado); la sesión sigue en memoria */
    }
  }

  function cerrarSesion() {
    token.value = null
    nombre.value = null
    try {
      localStorage.removeItem('planta_token')
      localStorage.removeItem('planta_nombre')
    } catch {
      /* nada que limpiar si no había storage */
    }
  }

  return { token, nombre, iniciarSesion, cerrarSesion }
})
