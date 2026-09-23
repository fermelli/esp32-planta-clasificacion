<script setup lang="ts">
import { cn } from '@/lib/utils'
const props = defineProps<{ class?: string }>()
</script>

<template>
  <h3 :class="cn('text-sm font-medium leading-none text-muted-foreground', props.class)">
    <slot />
  </h3>
</template>
