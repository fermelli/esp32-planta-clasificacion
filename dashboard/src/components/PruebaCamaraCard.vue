<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { API_URL, apiFetch } from '@/lib/api'
import { useLiveStore } from '@/stores/live'
import type { PruebaCamara } from '@/lib/types'
import { Card, CardHeader, CardTitle, CardContent, CardDescription } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Badge } from '@/components/ui/badge'
import { Camera, TriangleAlert } from 'lucide-vue-next'

const props = defineProps<{ version: 'rostro' | 'color' }>()
const live = useLiveStore()

const esperando = ref(false)
const error = ref('')
let plazo: ReturnType<typeof setTimeout> | undefined

const online = computed(() => live.camaras?.[props.version].online ?? false)
const prueba = computed<PruebaCamara | undefined>(() => live.pruebas[props.version])

onMounted(async () => {
  if (prueba.value) return
  try {
    const ultima = await apiFetch<PruebaCamara | null>(`/api/camara/prueba/${props.version}`)
    if (ultima) live.pruebas = { ...live.pruebas, [props.version]: ultima }
  } catch {
    // sin foto previa: la tarjeta queda vacía
  }
})
onBeforeUnmount(() => clearTimeout(plazo))

// La foto llega por WebSocket: en cuanto aparece una nueva se corta la espera.
watch(
  () => prueba.value?.ts,
  () => {
    if (!esperando.value) return
    esperando.value = false
    clearTimeout(plazo)
  },
)

async function probar() {
  error.value = ''
  esperando.value = true
  try {
    await apiFetch(`/api/camara/probar/${props.version}`, { method: 'POST' })
    plazo = setTimeout(() => {
      if (esperando.value) {
        esperando.value = false
        error.value =
          'La placa no respondió en 15 s. Revisá el monitor serie: puede ser la IP de la laptop en API_URL, el firewall o el CAMARA_TOKEN.'
      }
    }, 15000)
  } catch (e) {
    esperando.value = false
    error.value = e instanceof Error ? e.message : 'No se pudo pedir la foto'
  }
}

function nivelBrillo(b: number) {
  return b < 60 ? 'oscura' : b > 200 ? 'sobreexpuesta' : 'bien'
}
const rgb = (c: number[] | null) => (c ? `rgb(${c[0]}, ${c[1]}, ${c[2]})` : 'transparent')
</script>

<template>
  <Card>
    <CardHeader>
      <CardTitle class="flex items-center justify-between">
        <span class="flex items-center gap-2">
          <Camera class="h-4 w-4 text-muted-foreground" stroke-width="2" />
          Probar la cámara
        </span>
        <Button size="sm" class="gap-1.5" :disabled="!online || esperando" @click="probar">
          <Camera class="h-3.5 w-3.5" stroke-width="2" />
          {{ esperando ? 'Esperando la foto...' : 'Sacar foto de prueba' }}
        </Button>
      </CardTitle>
      <CardDescription>
        La placa saca una foto y el servidor la analiza. Sirve para comprobar que el lente funciona,
        hacia dónde apunta y si la luz es buena, sin tener que loguearse ni pasar cajas.
        <span v-if="!online" class="font-medium text-destructive">
          La cámara de {{ version }} no está conectada.
        </span>
      </CardDescription>
    </CardHeader>
    <CardContent class="flex flex-col gap-4">
      <p v-if="error" class="text-sm text-destructive">{{ error }}</p>

      <div v-if="prueba" class="grid grid-cols-1 gap-4 lg:grid-cols-2">
        <img
          :src="`${API_URL}/capturas/${prueba.imagen}?t=${prueba.ts}`"
          alt="foto de prueba"
          class="w-full rounded-lg border border-border object-cover"
        />
        <div class="flex flex-col gap-3 text-sm">
          <dl class="grid grid-cols-2 gap-x-4 gap-y-1">
            <dt class="text-muted-foreground">Resolución</dt>
            <dd class="tabular-nums">{{ prueba.ancho }} × {{ prueba.alto }}</dd>
            <dt class="text-muted-foreground">Peso</dt>
            <dd class="tabular-nums">{{ prueba.kb }} KB</dd>
            <dt class="text-muted-foreground">Brillo</dt>
            <dd class="tabular-nums">
              {{ prueba.brillo }} / 255
              <span class="text-muted-foreground">({{ nivelBrillo(prueba.brillo) }})</span>
            </dd>
            <dt class="text-muted-foreground">Nitidez</dt>
            <dd class="tabular-nums">{{ prueba.nitidez }}</dd>
            <dt class="text-muted-foreground">Hora</dt>
            <dd>{{ new Date(prueba.ts).toLocaleTimeString() }}</dd>
          </dl>

          <div v-for="aviso in prueba.avisos" :key="aviso" class="flex items-start gap-2">
            <TriangleAlert class="mt-0.5 h-4 w-4 shrink-0 text-amber-500" stroke-width="2" />
            <span>{{ aviso }}</span>
          </div>
          <Badge v-if="!prueba.avisos.length" variant="success" class="w-fit">
            imagen y luz razonables
          </Badge>

          <div v-if="prueba.rostro" class="flex flex-col gap-1 border-t border-border pt-3">
            <p>
              Caras detectadas:
              <b>{{ prueba.rostro.caras }}</b>
              <span v-if="prueba.rostro.score !== null" class="text-muted-foreground">
                (confianza {{ prueba.rostro.score }})
              </span>
            </p>
            <p v-if="prueba.rostro.caras === 0" class="text-muted-foreground">
              No se ve ninguna cara: acercate, mirá al lente de frente y con luz de frente.
            </p>
            <template v-if="prueba.rostro.coincidencias.length">
              <p class="text-muted-foreground">Se parece a (umbral {{ prueba.rostro.umbral }}):</p>
              <p
                v-for="c in prueba.rostro.coincidencias"
                :key="c.nombre"
                class="flex items-center gap-2"
              >
                <b>{{ c.nombre }}</b>
                <span class="tabular-nums">{{ c.similitud.toFixed(3) }}</span>
                <Badge :variant="c.similitud >= prueba.rostro.umbral ? 'success' : 'outline'">
                  {{ c.similitud >= prueba.rostro.umbral ? 'coincidiría' : 'no coincidiría' }}
                </Badge>
              </p>
            </template>
            <p v-else-if="prueba.rostro.caras > 0" class="text-muted-foreground">
              Todavía no hay caras enroladas para comparar.
            </p>
          </div>

          <div v-if="prueba.color" class="flex flex-col gap-2 border-t border-border pt-3">
            <p class="flex items-center gap-2">
              Color medio de la zona central:
              <span
                class="inline-block h-5 w-8 rounded border border-border"
                :style="{ backgroundColor: rgb(prueba.color.rgb_medio) }"
              />
              <span class="text-xs tabular-nums text-muted-foreground">
                {{ prueba.color.rgb_medio?.join(', ') }}
              </span>
            </p>
            <p v-if="prueba.color.color_ia">
              La IA dice: <b>{{ prueba.color.color_ia }}</b>
              <span v-if="prueba.color.confianza !== null" class="text-muted-foreground">
                ({{ (prueba.color.confianza * 100).toFixed(0) }}%)
              </span>
            </p>
            <p v-else class="text-muted-foreground">Todavía no hay un modelo entrenado.</p>
          </div>
        </div>
      </div>
      <p v-else class="py-4 text-center text-sm text-muted-foreground">
        Todavía no hay ninguna foto de prueba.
      </p>
    </CardContent>
  </Card>
</template>
