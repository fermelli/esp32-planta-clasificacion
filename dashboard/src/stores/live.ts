import { defineStore } from 'pinia'
import { ref } from 'vue'
import { WS_URL, apiFetch } from '@/lib/api'
import type { WsMensaje, SorterEstado, CamaraConfig } from '@/lib/types'

const MAX_BUFFER = 30

export const useLiveStore = defineStore('live', () => {
  const conectado = ref(false)
  const sorterEstado = ref<SorterEstado>({})
  const ultimosEventos = ref<Array<WsMensaje & { type: 'evento_caja' }>>([])
  const ultimasAlertas = ref<Array<WsMensaje & { type: 'alerta' }>>([])
  const ultimosIntentos = ref<Array<WsMensaje & { type: 'intento_login' }>>([])
  const ultimasVerificaciones = ref<Array<WsMensaje & { type: 'verificacion_rostro' }>>([])
  const ultimasClasificaciones = ref<Array<WsMensaje & { type: 'clasificacion_camara' }>>([])

  // Qué versiones de la cámara están activadas y cuáles tienen una placa conectada.
  const camaras = ref<CamaraConfig | null>(null)

  async function cargarCamaras() {
    try {
      camaras.value = await apiFetch<CamaraConfig>('/api/camara/config')
    } catch {
      // sin sesión o servidor caído: el menú simplemente no muestra las cámaras
    }
  }

  let socket: WebSocket | null = null
  let reintentoMs = 1000

  function conectar() {
    if (
      socket &&
      (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)
    )
      return

    socket = new WebSocket(WS_URL)

    socket.onopen = () => {
      conectado.value = true
      cargarCamaras() // el estado pudo cambiar mientras estaba desconectado
      reintentoMs = 1000
    }

    socket.onclose = () => {
      conectado.value = false
      setTimeout(conectar, reintentoMs)
      reintentoMs = Math.min(reintentoMs * 2, 15000)
    }

    socket.onerror = () => socket?.close()

    socket.onmessage = (ev) => {
      let msg: WsMensaje
      try {
        msg = JSON.parse(ev.data)
      } catch {
        return
      }

      if (msg.type === 'sorter_estado') {
        sorterEstado.value = { ...sorterEstado.value, ...msg }
      } else if (msg.type === 'evento_caja') {
        ultimosEventos.value = [msg, ...ultimosEventos.value].slice(0, MAX_BUFFER)
      } else if (msg.type === 'alerta') {
        ultimasAlertas.value = [msg, ...ultimasAlertas.value].slice(0, MAX_BUFFER)
      } else if (msg.type === 'intento_login') {
        ultimosIntentos.value = [msg, ...ultimosIntentos.value].slice(0, MAX_BUFFER)
      } else if (msg.type === 'verificacion_rostro') {
        ultimasVerificaciones.value = [msg, ...ultimasVerificaciones.value].slice(0, MAX_BUFFER)
      } else if (msg.type === 'camara_estado') {
        const actual = camaras.value?.[msg.version]
        if (actual && camaras.value) {
          camaras.value = {
            ...camaras.value,
            [msg.version]: { ...actual, online: msg.online, activo: actual.flag && msg.online },
          }
        }
      } else if (msg.type === 'clasificacion_camara') {
        ultimasClasificaciones.value = [msg, ...ultimasClasificaciones.value].slice(0, MAX_BUFFER)
      }
    }
  }

  return {
    conectado,
    sorterEstado,
    ultimosEventos,
    ultimasAlertas,
    ultimosIntentos,
    ultimasVerificaciones,
    ultimasClasificaciones,
    camaras,
    conectar,
  }
})
