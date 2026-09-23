import { defineStore } from 'pinia'
import { ref } from 'vue'
import { WS_URL } from '@/lib/api'
import type { WsMensaje, SorterEstado } from '@/lib/types'

const MAX_BUFFER = 30

export const useLiveStore = defineStore('live', () => {
  const conectado = ref(false)
  const sorterEstado = ref<SorterEstado>({})
  const ultimosEventos = ref<Array<WsMensaje & { type: 'evento_caja' }>>([])
  const ultimasAlertas = ref<Array<WsMensaje & { type: 'alerta' }>>([])
  const ultimosIntentos = ref<Array<WsMensaje & { type: 'intento_login' }>>([])

  let socket: WebSocket | null = null
  let reintentoMs = 1000

  function conectar() {
    if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return

    socket = new WebSocket(WS_URL)

    socket.onopen = () => {
      conectado.value = true
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
      }
    }
  }

  return { conectado, sorterEstado, ultimosEventos, ultimasAlertas, ultimosIntentos, conectar }
})
