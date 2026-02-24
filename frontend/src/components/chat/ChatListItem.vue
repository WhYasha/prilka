<template>
  <div
    class="chat-item"
    :class="{ active }"
    @click="emit('select')"
    @contextmenu.prevent="emit('contextmenu', $event)"
  >
    <Avatar :name="displayName" :url="chat.other_avatar_url" size="sm" />
    <div class="chat-item-info">
      <div class="chat-item-top">
        <span class="chat-item-name">
          <span v-if="chat.type === 'channel'" class="ci-channel" />
          <span v-else-if="chat.type === 'group'" class="ci-group" />
          {{ displayName }}
          <span v-if="chat.is_muted" class="mute-icon" title="Muted">&#128276;</span>
        </span>
        <span v-if="chat.unread_count > 0" class="unread-badge">{{ chat.unread_count > 99 ? '99+' : chat.unread_count }}</span>
        <span class="chat-item-time">{{ formatTime(chat.last_at || chat.updated_at) }}</span>
      </div>
      <div class="chat-item-preview">{{ chat.last_message || '' }}</div>
    </div>
    <button
      class="star-btn"
      :class="{ 'star-active': chat.is_favorite }"
      :title="chat.is_favorite ? 'Unstar' : 'Star'"
      @click.stop="emit('toggleFavorite')"
    >&#9733;</button>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import Avatar from '@/components/ui/Avatar.vue'
import type { Chat } from '@/api/types'

const props = defineProps<{
  chat: Chat
  active: boolean
}>()

const emit = defineEmits<{
  select: []
  toggleFavorite: []
  contextmenu: [e: MouseEvent]
}>()

const displayName = computed(() => {
  const c = props.chat
  if (c.type === 'channel' || c.type === 'group') return c.title || c.name || 'Untitled'
  return c.other_display_name || c.other_username || c.title || c.name || 'Chat'
})

function formatTime(isoStr: string | undefined): string {
  if (!isoStr) return ''
  const d = new Date(isoStr.endsWith('Z') ? isoStr : isoStr + 'Z')
  const now = new Date()
  if (d.toDateString() === now.toDateString()) {
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
  }
  return d.toLocaleDateString([], { month: 'short', day: 'numeric' })
}
</script>
