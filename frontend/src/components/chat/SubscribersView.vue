<template>
  <div class="subscribers-view">
    <div class="view-header">
      <button class="btn btn-sm btn-ghost" @click="emit('back')">← Back</button>
      <span class="view-title">Subscribers</span>
    </div>

    <div class="search-bar">
      <input
        v-model="search"
        type="text"
        class="input"
        placeholder="Search members..."
      />
    </div>

    <div v-if="filtered.length === 0" class="member-empty">
      {{ search ? 'No members found' : 'No subscribers' }}
    </div>

    <div v-else class="member-list">
      <div v-for="member in filtered" :key="member.id" class="member-item">
        <Avatar
          :name="member.display_name || member.username"
          :url="member.avatar_url"
          size="md"
        />
        <div class="member-info">
          <span class="member-name">{{ member.display_name || member.username }}</span>
          <span class="role-badge" :class="`role-badge--${member.role}`">{{ member.role }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { useChannelStore } from '@/stores/channel'
import Avatar from '@/components/ui/Avatar.vue'

defineProps<{ chatId: number }>()
const emit = defineEmits<{ back: [] }>()

const channelStore = useChannelStore()
const search = ref('')

const filtered = computed(() => {
  const q = search.value.toLowerCase().trim()
  if (!q) return channelStore.members
  return channelStore.members.filter(
    (m) =>
      m.username.toLowerCase().includes(q) ||
      (m.display_name && m.display_name.toLowerCase().includes(q)),
  )
})
</script>

<style scoped>
.subscribers-view {
  display: flex;
  flex-direction: column;
  height: 100%;
}

.view-header {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  padding: 0.75rem 1rem;
  border-bottom: 1px solid var(--border);
}

.view-title {
  font-weight: 600;
  font-size: 1rem;
}

.search-bar {
  padding: 0.5rem 1rem;
}

.search-bar .input {
  width: 100%;
  padding: 0.4rem 0.75rem;
  border: 1px solid var(--border);
  border-radius: 6px;
  font-size: 0.85rem;
  background: var(--bg-secondary, transparent);
  color: inherit;
}

.member-empty {
  padding: 1rem;
  text-align: center;
  color: var(--text-secondary);
}

.member-list {
  display: flex;
  flex-direction: column;
  overflow-y: auto;
}

.member-item {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.5rem 1rem;
}

.member-item:hover {
  background: var(--hover);
}

.member-info {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.member-name {
  font-size: 0.9rem;
  font-weight: 500;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.role-badge {
  font-size: 0.7rem;
  font-weight: 600;
  text-transform: uppercase;
  color: var(--text-secondary);
}

.role-badge--owner {
  color: var(--accent, #7c3aed);
}

.role-badge--admin {
  color: var(--primary, #3b82f6);
}
</style>
