<template>
  <div class="avatar-wrap" :class="sizeClass">
    <div class="avatar" :class="sizeClass">
      <img v-if="url" :src="url" :alt="name" />
      <template v-else>{{ initial }}</template>
    </div>
    <span v-if="online" class="online-dot" />
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = withDefaults(defineProps<{
  name: string
  url?: string | null
  size?: 'sm' | 'md' | 'lg' | 'xl' | 'xxl'
  online?: boolean | null
}>(), {
  url: null,
  size: 'md',
  online: null,
})

const initial = computed(() => (props.name ?? '?')[0]?.toUpperCase() ?? '?')

const sizeClass = computed(() => {
  if (props.size === 'sm') return 'avatar-sm'
  if (props.size === 'lg') return 'avatar-lg'
  if (props.size === 'xl') return 'avatar-xl'
  if (props.size === 'xxl') return 'avatar-xxl'
  return ''
})
</script>
