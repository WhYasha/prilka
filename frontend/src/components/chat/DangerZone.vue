<template>
  <div class="danger-zone">
    <h3 class="danger-title">Danger Zone</h3>

    <button class="btn btn-danger-outline" @click="handleLeave">
      Leave Channel
    </button>

    <button v-if="channelStore.isOwner" class="btn btn-danger" @click="showDeleteConfirm = true">
      Delete Channel
    </button>

    <!-- Delete confirmation dialog -->
    <div v-if="showDeleteConfirm" class="modal-backdrop" @click.self="showDeleteConfirm = false">
      <div class="modal modal-sm">
        <div class="modal-header">
          <h2 class="modal-title">Delete Channel</h2>
          <button class="icon-btn" @click="showDeleteConfirm = false">&#10005;</button>
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
            {{ deleting ? 'Deleting…' : 'Delete' }}
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
import * as chatsApi from '@/api/chats'

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

const showDeleteConfirm = ref(false)
const deleting = ref(false)

async function handleLeave() {
  try {
    await chatsStore.leave(props.chatId)
    emit('left')
  } catch (e) {
    console.error('Failed to leave channel', e)
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
  } catch (e) {
    console.error('Failed to delete channel', e)
  } finally {
    deleting.value = false
  }
}
</script>

<style scoped>
.danger-zone {
  padding: 1.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

.danger-title {
  color: var(--color-danger, #e53935);
  font-size: 1rem;
  font-weight: 600;
  margin: 0 0 0.5rem;
}

.btn-danger {
  background: var(--color-danger, #e53935);
  color: #fff;
  border: none;
  border-radius: 0.5rem;
  padding: 0.5rem 1.25rem;
  cursor: pointer;
  font-weight: 500;
  font-size: 0.9rem;
}

.btn-danger:hover {
  background: var(--color-danger-hover, #c62828);
}

.btn-danger:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.btn-danger-outline {
  background: transparent;
  color: var(--color-danger, #e53935);
  border: 1px solid var(--color-danger, #e53935);
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
  color: var(--text-primary);
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
