<template>
  <div class="invite-links-view">
    <div class="view-header">
      <button class="icon-btn" aria-label="Back" @click="emit('back')">
        <ArrowLeft :size="20" :stroke-width="2" />
      </button>
      <span class="view-title">Invite Links</span>
    </div>

    <div style="padding: 0 1rem .5rem">
      <button class="btn btn-sm btn-outline" @click="handleCreate" :disabled="creating">
        {{ creating ? 'Creating...' : 'Create Invite Link' }}
      </button>
    </div>

    <div v-if="store.loading" class="invite-loading" style="padding: 1rem; text-align: center">
      Loading...
    </div>

    <div v-else-if="store.invites.length === 0" style="padding: 1rem; text-align: center; color: var(--text-muted)">
      No active invite links
    </div>

    <div v-else class="invite-list" style="padding: 0 1rem">
      <div v-for="invite in store.invites" :key="invite.id" class="invite-item">
        <div class="invite-item-info">
          <code class="invite-token">{{ invite.token }}</code>
          <span class="invite-date">{{ formatDate(invite.created_at) }}</span>
        </div>
        <div class="invite-item-actions">
          <button class="icon-btn" title="Copy link" @click="copyLink(invite.token)">
            <Copy :size="16" :stroke-width="2" />
          </button>
          <button class="btn btn-sm btn-ghost btn-danger" @click="handleRevoke(invite.token)">
            Revoke
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useInvitesStore } from '@/stores/invites'
import { useToast } from '@/composables/useToast'
import { ArrowLeft, Copy } from 'lucide-vue-next'

const props = defineProps<{ chatId: number }>()
const emit = defineEmits<{ back: [] }>()

const store = useInvitesStore()
const { showToast } = useToast()
const creating = ref(false)

function formatDate(dateStr: string): string {
  return new Date(dateStr).toLocaleDateString(undefined, {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
  })
}

function copyLink(token: string) {
  const url = `${window.location.origin}/join/${token}`
  if (navigator.clipboard) {
    navigator.clipboard.writeText(url).then(() => showToast('Link copied!'))
  }
}

async function handleCreate() {
  creating.value = true
  try {
    await store.createInvite(props.chatId)
    showToast('Invite link created')
  } catch {
    showToast('Failed to create invite')
  } finally {
    creating.value = false
  }
}

async function handleRevoke(token: string) {
  try {
    await store.revokeInvite(token)
    showToast('Invite revoked')
  } catch {
    showToast('Failed to revoke invite')
  }
}

onMounted(() => store.loadInvites(props.chatId))
</script>

<style scoped>
.invite-links-view {
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

.invite-list {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
  overflow-y: auto;
}

.invite-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0.5rem;
  border: 1px solid var(--border);
  border-radius: 6px;
}

.invite-item-info {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
  min-width: 0;
}

.invite-token {
  font-size: 0.85rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.invite-date {
  font-size: 0.75rem;
  color: var(--text-muted);
}

.invite-item-actions {
  display: flex;
  gap: 0.25rem;
  flex-shrink: 0;
}
</style>
