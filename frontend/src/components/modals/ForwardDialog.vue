<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal">
      <div class="modal-header">
        <h2 class="modal-title">Forward Message</h2>
        <button class="icon-btn" @click="emit('close')">&#10005;</button>
      </div>
      <div class="modal-body">
        <input
          v-model="searchQuery"
          class="search-input"
          type="search"
          placeholder="Search chats..."
          @input="onSearch"
        />
        <div class="forward-chat-list">
          <div v-if="filteredChats.length === 0" class="empty-state-small">No chats found.</div>
          <div
            v-for="chat in filteredChats"
            :key="chat.id"
            class="forward-chat-item"
            :class="{ selected: selectedChatId === chat.id }"
            @click="selectedChatId = chat.id"
          >
            <Avatar :name="chatDisplayName(chat)" :url="chatAvatarUrl(chat)" size="sm" />
            <div class="forward-chat-info">
              <div class="forward-chat-name">{{ chatDisplayName(chat) }}</div>
              <div class="forward-chat-type">{{ chat.type }}</div>
            </div>
            <span v-if="selectedChatId === chat.id" class="forward-check">&#10003;</span>
          </div>
        </div>
        <div class="modal-actions">
          <button
            class="btn btn-primary"
            :disabled="selectedChatId === null || forwarding"
            @click="handleForward"
          >
            {{ forwarding ? 'Forwarding...' : 'Forward' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { useChatsStore } from '@/stores/chats'
import { useMessagesStore } from '@/stores/messages'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import type { Chat } from '@/api/types'

const props = defineProps<{
  messageIds: number[]
  fromChatId: number
}>()

const emit = defineEmits<{
  close: []
  forwarded: [chatId: number]
}>()

const chatsStore = useChatsStore()
const messagesStore = useMessagesStore()
const { showToast } = useToast()

const searchQuery = ref('')
const selectedChatId = ref<number | null>(null)
const forwarding = ref(false)

const filteredChats = computed(() => {
  const q = searchQuery.value.toLowerCase().trim()
  const chats = chatsStore.sortedChats
  if (!q) return chats
  return chats.filter((chat) => chatDisplayName(chat).toLowerCase().includes(q))
})

function chatDisplayName(chat: Chat): string {
  if (chat.type === 'direct') {
    return chat.other_display_name || chat.other_username || chat.title || 'Direct Chat'
  }
  return chat.title || chat.name || 'Unnamed Chat'
}

function chatAvatarUrl(chat: Chat): string | undefined {
  if (chat.type === 'direct') {
    return chat.other_avatar_url ?? undefined
  }
  return undefined
}

function onSearch() {
  // Reactive via v-model, no debounce needed for local filtering
}

async function handleForward() {
  if (selectedChatId.value === null) return
  forwarding.value = true
  try {
    await messagesStore.forwardMessages(selectedChatId.value, props.fromChatId, props.messageIds)
    showToast('Message forwarded!')
    emit('forwarded', selectedChatId.value)
  } catch {
    showToast('Failed to forward message')
  } finally {
    forwarding.value = false
  }
}
</script>

<style scoped>
.forward-chat-list {
  max-height: 360px;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 2px;
  margin: 12px 0;
}

.forward-chat-item {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 10px 12px;
  border-radius: 8px;
  cursor: pointer;
  transition: background 0.15s;
}

.forward-chat-item:hover {
  background: var(--color-bg-hover, rgba(0, 0, 0, 0.05));
}

.forward-chat-item.selected {
  background: var(--color-primary-light, rgba(58, 130, 246, 0.12));
}

.forward-chat-info {
  flex: 1;
  min-width: 0;
}

.forward-chat-name {
  font-weight: 500;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.forward-chat-type {
  font-size: 0.8rem;
  color: var(--color-text-secondary, #888);
  text-transform: capitalize;
}

.forward-check {
  color: var(--color-primary, #3a82f6);
  font-size: 1.2rem;
  font-weight: bold;
}
</style>
