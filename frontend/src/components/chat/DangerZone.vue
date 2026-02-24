<template>
  <div class="danger-zone">
    <div class="view-header">
      <button class="icon-btn" aria-label="Back" @click="emit('back')">
        <ArrowLeft :size="20" :stroke-width="2" />
      </button>
      <span class="view-title">Danger Zone</span>
    </div>

    <div class="danger-zone-body">
      <button class="btn btn-danger-outline" @click="handleLeave">
        <LogOut :size="18" :stroke-width="1.8" />
        Leave Channel
      </button>

      <button v-if="channelStore.isOwner" class="btn btn-danger" @click="showDeleteConfirm = true">
        <Trash2 :size="18" :stroke-width="1.8" />
        Delete Channel
      </button>
    </div>

    <!-- Delete confirmation dialog -->
    <div v-if="showDeleteConfirm" class="modal-backdrop" @click.self="showDeleteConfirm = false">
      <div class="modal modal-sm">
        <div class="modal-header">
          <h2 class="modal-title">Delete Channel</h2>
          <button class="icon-btn" aria-label="Close" @click="showDeleteConfirm = false">
            <X :size="20" :stroke-width="2" />
          </button>
        </div>
        <div class="modal-body">
          <p class="delete-confirm-text">
            Are you sure you want to delete this channel? This action cannot be undone.
            All messages and members will be removed.
          </p>
        </div>
        <div class="modal-actions">
          <button class="btn btn-ghost" @click="showDeleteConfirm = false">Cancel</button>
          <button class="btn btn-danger" :disabled="deleting" @click="handleDelete">
            {{ deleting ? 'Deleting...' : 'Delete' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useChatsStore } from '@/stores/chats'
import { useChannelStore } from '@/stores/channel'
import { useToast } from '@/composables/useToast'
import * as chatsApi from '@/api/chats'
import { ArrowLeft, LogOut, Trash2, X } from 'lucide-vue-next'

const props = defineProps<{
  chatId: number
}>()

const emit = defineEmits<{
  back: []
  left: []
  deleted: []
}>()

const chatsStore = useChatsStore()
const channelStore = useChannelStore()
const { showToast } = useToast()

const showDeleteConfirm = ref(false)
const deleting = ref(false)

async function handleLeave() {
  try {
    await chatsStore.leave(props.chatId)
    emit('left')
  } catch {
    showToast('Failed to leave channel')
  }
}

async function handleDelete() {
  deleting.value = true
  try {
    await chatsApi.deleteChat(props.chatId)
    chatsStore.chats = chatsStore.chats.filter((c) => c.id !== props.chatId)
    if (chatsStore.activeChatId === props.chatId) {
      chatsStore.setActiveChat(null)
    }
    showDeleteConfirm.value = false
    emit('deleted')
  } catch {
    showToast('Failed to delete channel')
  } finally {
    deleting.value = false
  }
}
</script>

<style scoped>
.danger-zone {
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
  color: var(--danger);
}

.danger-zone-body {
  padding: 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.btn-danger {
  display: flex;
  align-items: center;
  gap: 8px;
  background: var(--danger);
  color: #fff;
  border: none;
  border-radius: 0.5rem;
  padding: 0.5rem 1.25rem;
  cursor: pointer;
  font-weight: 500;
  font-size: 0.9rem;
}

.btn-danger:hover {
  opacity: 0.9;
}

.btn-danger:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.btn-danger-outline {
  display: flex;
  align-items: center;
  gap: 8px;
  background: transparent;
  color: var(--danger);
  border: 1px solid var(--danger);
  border-radius: 0.5rem;
  padding: 0.5rem 1.25rem;
  cursor: pointer;
  font-weight: 500;
  font-size: 0.9rem;
}

.btn-danger-outline:hover {
  background: rgba(229, 57, 53, 0.1);
}

.modal-sm {
  max-width: 400px;
}

.delete-confirm-text {
  margin: 0;
  color: var(--text);
  font-size: 0.95rem;
  line-height: 1.5;
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.5rem;
  padding: 1rem;
}
</style>
