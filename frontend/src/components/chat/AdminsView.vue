<template>
  <div class="admins-view">
    <div class="view-header">
      <button class="icon-btn" aria-label="Back" @click="emit('back')">
        <ArrowLeft :size="20" :stroke-width="2" />
      </button>
      <span class="view-title">Administrators</span>
    </div>

    <!-- Adding mode: show non-admin members to pick from -->
    <template v-if="adding">
      <div class="search-bar">
        <input
          v-model="search"
          type="text"
          class="form-input"
          placeholder="Search members..."
        />
      </div>

      <div v-if="filteredNonAdmins.length === 0" class="member-empty">
        {{ search ? 'No members found' : 'No members to promote' }}
      </div>

      <div v-else class="member-list">
        <div
          v-for="member in filteredNonAdmins"
          :key="member.id"
          class="member-item member-item--selectable"
          @click="handlePromote(member.id)"
        >
          <Avatar
            :name="member.display_name || member.username"
            :url="member.avatar_url"
            size="md"
          />
          <div class="member-info">
            <span class="member-name">{{ member.display_name || member.username }}</span>
            <span class="role-badge">{{ member.role }}</span>
          </div>
        </div>
      </div>

      <div class="view-footer">
        <button class="btn btn--secondary" @click="adding = false">Cancel</button>
      </div>
    </template>

    <!-- Normal mode: show admins list -->
    <template v-else>
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
          <button
            v-if="channelStore.isOwner && member.role === 'admin'"
            class="btn btn--small btn--danger"
            @click="handleDemote(member.id)"
          >
            Demote
          </button>
        </div>
      </div>

      <div v-if="channelStore.isOwner" class="view-footer">
        <button class="btn btn--primary" @click="adding = true">Add Admin</button>
      </div>
    </template>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { useChannelStore } from '@/stores/channel'
import Avatar from '@/components/ui/Avatar.vue'
import { ArrowLeft } from 'lucide-vue-next'
import { promoteMember, demoteMember } from '@/api/channel'

const props = defineProps<{ chatId: number }>()
const emit = defineEmits<{ back: [] }>()

const channelStore = useChannelStore()
const adding = ref(false)
const search = ref('')

const admins = computed(() =>
  channelStore.members.filter((m) => m.role === 'admin' || m.role === 'owner'),
)

const nonAdmins = computed(() =>
  channelStore.members.filter((m) => m.role === 'member'),
)

const filteredNonAdmins = computed(() => {
  const q = search.value.toLowerCase().trim()
  if (!q) return nonAdmins.value
  return nonAdmins.value.filter(
    (m) =>
      m.username.toLowerCase().includes(q) ||
      (m.display_name && m.display_name.toLowerCase().includes(q)),
  )
})

async function handlePromote(userId: number) {
  try {
    await promoteMember(props.chatId, userId)
    const member = channelStore.members.find((m) => m.id === userId)
    if (member) member.role = 'admin'
    adding.value = false
    search.value = ''
  } catch {
    // silently handle - API errors shown by interceptor
  }
}

async function handleDemote(userId: number) {
  try {
    await demoteMember(props.chatId, userId)
    const member = channelStore.members.find((m) => m.id === userId)
    if (member) member.role = 'member'
  } catch {
    // silently handle - API errors shown by interceptor
  }
}
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

.search-bar {
  padding: 0.5rem 1rem;
}

.search-bar .form-input {
  width: 100%;
}

.member-empty {
  padding: 1rem;
  text-align: center;
  color: var(--text-muted);
}

.member-list {
  display: flex;
  flex-direction: column;
  overflow-y: auto;
  flex: 1;
}

.member-item {
  display: flex;
  align-items: center;
  gap: 0.75rem;
  padding: 0.5rem 1rem;
}

.member-item:hover {
  background: var(--input-bg);
}

.member-item--selectable {
  cursor: pointer;
}

.member-info {
  display: flex;
  flex-direction: column;
  min-width: 0;
  flex: 1;
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
  color: var(--text-muted);
}

.role-badge--owner {
  color: var(--accent);
}

.role-badge--admin {
  color: var(--accent);
}

.view-footer {
  padding: 0.75rem 1rem;
  border-top: 1px solid var(--border);
}

.view-footer .btn {
  width: 100%;
}

.btn--small {
  padding: 0.25rem 0.5rem;
  font-size: 0.75rem;
  margin-left: auto;
}

.btn--danger {
  color: var(--danger, #e53e3e);
  background: transparent;
  border: 1px solid var(--danger, #e53e3e);
}

.btn--danger:hover {
  background: var(--danger, #e53e3e);
  color: white;
}
</style>
