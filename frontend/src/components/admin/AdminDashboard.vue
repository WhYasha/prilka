<template>
  <div>
    <h1 class="admin-page-title">Dashboard</h1>
    <p v-if="loading">Loading stats...</p>
    <template v-else-if="stats">
      <div class="admin-stats-grid">
        <div v-for="card in statCards" :key="card.label" class="admin-stat-card">
          <div class="stat-value">{{ card.value }}</div>
          <div class="stat-label">{{ card.label }}</div>
        </div>
      </div>

      <!-- Chats by type -->
      <div v-if="stats.chats_by_type" class="admin-stats-grid">
        <div v-for="(count, type) in stats.chats_by_type" :key="type" class="admin-stat-card">
          <div class="stat-value">{{ count }}</div>
          <div class="stat-label">
            <span class="badge" :class="'badge-' + type">{{ type }}</span>
          </div>
        </div>
      </div>

      <!-- Bar charts -->
      <BarChart title="New Registrations (14 days)" :data="stats.reg_trend || []" />
      <BarChart title="Messages (14 days)" :data="stats.msg_trend || []" />
    </template>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, computed } from 'vue'
import { getStats } from '@/api/admin'
import type { AdminStats } from '@/api/types'
import BarChart from './BarChart.vue'

const stats = ref<AdminStats | null>(null)
const loading = ref(true)

const statCards = computed(() => {
  if (!stats.value) return []
  return [
    { label: 'Total Users', value: stats.value.total_users },
    { label: 'DAU (24h)', value: stats.value.dau },
    { label: 'WAU (7d)', value: stats.value.wau },
    { label: 'New Today', value: stats.value.new_users_today },
    { label: 'Total Chats', value: stats.value.total_chats },
    { label: 'Total Messages', value: stats.value.total_messages },
    { label: 'Messages Today', value: stats.value.messages_today },
    { label: 'Blocked Users', value: stats.value.blocked_users },
  ]
})

onMounted(async () => {
  try {
    stats.value = await getStats()
  } finally {
    loading.value = false
  }
})
</script>
