<template>
  <aside class="sidebar">
    <header class="sidebar-header">
      <button class="icon-btn" aria-label="Menu" @click="emit('openDrawer')"><Menu :size="20" /></button>
      <div class="search-wrap">
        <input
          v-model="searchQuery"
          class="search-input"
          type="search"
          placeholder="Search chats..."
        />
      </div>
      <button
        class="icon-btn"
        aria-label="New chat"
        title="New chat"
        @click="emit('openNewChat')"
      ><SquarePen :size="20" /></button>
    </header>

    <div class="chat-list">
      <template v-if="filteredChats.length === 0">
        <div class="empty-state-small">
          No conversations yet.<br />Click the pencil icon to start one.
        </div>
      </template>
      <template v-else>
        <!-- Pinned section -->
        <div v-if="pinned.length > 0" class="chat-section-label">Pinned</div>
        <ChatListItem
          v-for="chat in pinned"
          :key="chat.id"
          :chat="chat"
          :active="chat.id === chatsStore.activeChatId"
          @select="emit('selectChat', chat.id)"
          @toggle-favorite="chatsStore.toggleFavorite(chat.id)"
          @toggle-pin="chatsStore.togglePin(chat.id)"
          @toggle-mute="chatsStore.toggleMute(chat.id)"
          @toggle-archive="chatsStore.toggleArchive(chat.id)"
          @contextmenu="showContextMenu($event, chat.id)"
        />

        <!-- Starred section -->
        <div v-if="favorites.length > 0" class="chat-section-label">Starred</div>
        <ChatListItem
          v-for="chat in favorites"
          :key="chat.id"
          :chat="chat"
          :active="chat.id === chatsStore.activeChatId"
          @select="emit('selectChat', chat.id)"
          @toggle-favorite="chatsStore.toggleFavorite(chat.id)"
          @toggle-pin="chatsStore.togglePin(chat.id)"
          @toggle-mute="chatsStore.toggleMute(chat.id)"
          @toggle-archive="chatsStore.toggleArchive(chat.id)"
          @contextmenu="showContextMenu($event, chat.id)"
        />

        <div v-if="(pinned.length > 0 || favorites.length > 0) && rest.length > 0" class="chat-section-label">
          All chats
        </div>
        <ChatListItem
          v-for="chat in rest"
          :key="chat.id"
          :chat="chat"
          :active="chat.id === chatsStore.activeChatId"
          @select="emit('selectChat', chat.id)"
          @toggle-favorite="chatsStore.toggleFavorite(chat.id)"
          @toggle-pin="chatsStore.togglePin(chat.id)"
          @toggle-mute="chatsStore.toggleMute(chat.id)"
          @toggle-archive="chatsStore.toggleArchive(chat.id)"
          @contextmenu="showContextMenu($event, chat.id)"
        />
      </template>
    </div>
  </aside>
</template>

<script setup lang="ts">
import { ref, computed, provide } from 'vue'
import { Menu, SquarePen } from 'lucide-vue-next'
import { useChatsStore } from '@/stores/chats'
import ChatListItem from '@/components/chat/ChatListItem.vue'

const emit = defineEmits<{
  openDrawer: []
  openNewChat: []
  selectChat: [chatId: number]
}>()

const chatsStore = useChatsStore()
const searchQuery = ref('')

const filteredChats = computed(() => {
  const q = searchQuery.value.toLowerCase()
  const nonArchived = chatsStore.sortedChats.filter((c) => !c.is_archived)
  if (!q) return nonArchived
  return nonArchived.filter((c) => {
    const name = chatDisplayName(c).toLowerCase()
    const uname = (c.other_username || '').toLowerCase()
    return name.includes(q) || uname.includes(q)
  })
})

const pinned = computed(() => filteredChats.value.filter((c) => c.is_pinned && !c.is_favorite))
const favorites = computed(() => filteredChats.value.filter((c) => c.is_favorite && !c.is_pinned))
const rest = computed(() => filteredChats.value.filter((c) => !c.is_favorite && !c.is_pinned))

function chatDisplayName(c: { type: string; title?: string | null; name?: string | null; other_display_name?: string; other_username?: string }): string {
  if (c.type === 'channel' || c.type === 'group') return c.title || c.name || 'Untitled'
  return c.other_display_name || c.other_username || c.title || c.name || 'Chat'
}

// Context menu
const contextChatId = ref<number | null>(null)
provide('contextChatId', contextChatId)

function showContextMenu(e: MouseEvent, chatId: number) {
  e.preventDefault()
  contextChatId.value = chatId
  // Dispatch custom event for ContextMenu component
  window.dispatchEvent(new CustomEvent('show-context-menu', {
    detail: { x: e.clientX, y: e.clientY, chatId },
  }))
}
</script>
