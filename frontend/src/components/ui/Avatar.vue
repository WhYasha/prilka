<template>
  <div class="avatar" :class="sizeClass">
    <img v-if="url" :src="url" :alt="name" />
    <template v-else>{{ initial }}</template>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(defineProps<{
  name: string
  url?: string | null
  size?: 'sm' | 'md' | 'lg' | 'xl'
}>(), {
  url: null,
  size: 'md',
})

const initial = computed(() => (props.name ?? '?')[0]?.toUpperCase() ?? '?')

const sizeClass = computed(() => {
  if (props.size === 'sm') return 'avatar-sm'
  if (props.size === 'lg') return 'avatar-lg'
  if (props.size === 'xl') return 'avatar-xl'
  return ''
})
</script>
