<template>
  <div class="admins-view">
    <div class="view-header">
      <button class="btn btn-sm btn-ghost" @click="emit('back')">← Back</button>
      <span class="view-title">Administrators</span>
    </div>

    <div v-if="admins.length === 0" class="member-empty">
      No administrators found
    </div>

    <div v-else class="member-list">
      <div v-for="member in admins" :key="member.id" class="member-item">
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
import { computed } from 'vue'
import { useChannelStore } from '@/stores/channel'
import Avatar from '@/components/ui/Avatar.vue'

defineProps<{ chatId: number }>()
const emit = defineEmits<{ back: [] }>()

const channelStore = useChannelStore()

const admins = computed(() =>
  channelStore.members.filter((m) => m.role === 'admin' || m.role === 'owner'),
)
</script>

<style scoped>
.admins-view {
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
