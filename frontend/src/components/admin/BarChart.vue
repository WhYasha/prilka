<template>
  <div v-if="data.length" class="admin-chart-section">
    <h3>{{ title }}</h3>
    <div class="bar-chart">
      <div v-for="d in data" :key="d.day" class="bar-row">
        <span class="bar-label">{{ shortDate(d.day) }}</span>
        <div class="bar-fill" :style="{ width: pct(d.count) + '%' }" />
        <span class="bar-value">{{ d.count }}</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'

const props = defineProps<{
  title: string
  data: Array<{ day: string; count: number }>
}>()

const max = computed(() => Math.max(...props.data.map((d) => d.count), 1))

function pct(count: number): number {
  return Math.round((count / max.value) * 100)
}

function shortDate(s: string): string {
  return s ? s.substring(5) : ''
}
</script>
