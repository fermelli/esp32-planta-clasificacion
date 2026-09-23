import { defineStore } from 'pinia'
import { ref, watch } from 'vue'

const CLAVE = 'tema'

function preferenciaInicial(): boolean {
  const guardado = localStorage.getItem(CLAVE)
  if (guardado) return guardado === 'oscuro'
  return window.matchMedia('(prefers-color-scheme: dark)').matches
}

export const useThemeStore = defineStore('theme', () => {
  const oscuro = ref(preferenciaInicial())

  watch(
    oscuro,
    (valor) => {
      document.documentElement.classList.toggle('dark', valor)
      localStorage.setItem(CLAVE, valor ? 'oscuro' : 'claro')
    },
    { immediate: true },
  )

  function alternar() {
    oscuro.value = !oscuro.value
  }

  return { oscuro, alternar }
})
