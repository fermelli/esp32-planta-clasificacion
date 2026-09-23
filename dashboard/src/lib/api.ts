import { useAuthStore } from '@/stores/auth'

const API_URL = import.meta.env.VITE_API_URL ?? 'http://localhost:8000'

export class ApiError extends Error {
  status: number
  constructor(status: number, message: string) {
    super(message)
    this.status = status
  }
}

export async function apiFetch<T>(path: string, opciones: RequestInit = {}): Promise<T> {
  const auth = useAuthStore()
  const headers = new Headers(opciones.headers)
  headers.set('Content-Type', 'application/json')
  if (auth.token) headers.set('Authorization', `Bearer ${auth.token}`)

  const res = await fetch(`${API_URL}${path}`, { ...opciones, headers })
  if (res.status === 401) {
    auth.cerrarSesion()
    throw new ApiError(401, 'Sesión expirada')
  }
  if (!res.ok) {
    const detalle = await res.json().catch(() => ({}))
    throw new ApiError(res.status, detalle.detail ?? `Error ${res.status}`)
  }
  return res.json() as Promise<T>
}

export const WS_URL = import.meta.env.VITE_WS_URL ?? 'ws://localhost:8000/ws'
