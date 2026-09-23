<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  valor: number
  colorEncendido: string
  label: string
}>()

// Mismo orden que mostrarContador() en el firmware del sorter: bit2 (MSB) a bit0 (LSB).
const bits = computed(() => [
  (props.valor >> 2) & 1,
  (props.valor >> 1) & 1,
  props.valor & 1,
])
</script>

<template>
  <div class="flex flex-col items-center gap-2">
    <div class="flex gap-1.5">
      <span
        v-for="(bit, i) in bits"
        :key="i"
        class="h-3.5 w-3.5 rounded-full border border-border transition-colors"
        :style="{ backgroundColor: bit ? colorEncendido : 'transparent' }"
      />
    </div>
    <span class="text-xs text-muted-foreground">{{ label }}</span>
    <span class="text-2xl font-semibold tabular-nums">{{ valor }}</span>
  </div>
</template>
