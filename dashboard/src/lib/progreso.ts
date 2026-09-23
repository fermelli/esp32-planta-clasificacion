import { ref } from 'vue'

// Estado global simple para la barra de progreso de navegación — no necesita
// ser un store de pinia porque el router la consume antes de que el árbol
// de componentes exista.
export const navegando = ref(false)
