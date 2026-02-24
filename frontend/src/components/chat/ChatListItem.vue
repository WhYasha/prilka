<template>
  <div
    ref="swipeRef"
    class="chat-item-swipe-wrapper"
    @touchstart="onTouchStart"
    @touchmove="onTouchMove"
    @touchend="onTouchEnd"
  >
    <div class="swipe-actions swipe-actions-left">
      <button class="swipe-action swipe-pin" @click="emit('togglePin')">
        {{ chat.is_pinned ? 'Unpin' : 'Pin' }}
      </button>
    </div>
    <div
      class="chat-item"
      :class="{ active, 'chat-item-unread': hasUnread }"
      :style="{ transform: `translateX(${swipeX}px)` }"
      @click="emit('select')"
      @contextmenu.prevent="emit('contextmenu', $event)"
    >
      <Avatar :name="displayName" :url="chat.other_avatar_url" size="sm" :online="isOnline" />
      <div class="chat-item-info">
        <div class="chat-item-top">
          <span class="chat-item-name">
            <span v-if="chat.is_pinned" class="pin-icon" title="Pinned">&#128204;</span>
            <span v-if="chat.type === 'channel'" class="ci-channel" />
            <span v-else-if="chat.type === 'group'" class="ci-group" />
            {{ displayName }}
            <span v-if="chat.is_muted" class="mute-icon" title="Muted">&#128276;</span>
          </span>
          <span class="chat-item-time">{{ formatTime(chat.last_at || chat.updated_at) }}</span>
        </div>
        <div class="chat-item-bottom">
          <span class="chat-item-preview">{{ chat.last_message || '' }}</span>
          <span
            v-if="chat.unread_count > 0"
            class="unread-badge"
            :class="{ 'unread-badge-muted': chat.is_muted }"
          >{{ chat.unread_count > 99 ? '99+' : chat.unread_count }}</span>
        </div>
      </div>
      <button
        class="star-btn"
        :class="{ 'star-active': chat.is_favorite }"
        :title="chat.is_favorite ? 'Unstar' : 'Star'"
        @click.stop="emit('toggleFavorite')"
      >&#9733;</button>
    </div>
    <div class="swipe-actions swipe-actions-right">
      <button class="swipe-action swipe-mute" @click="emit('toggleMute')">
        {{ chat.is_muted ? 'Unmute' : 'Mute' }}
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue'
import Avatar from '@/components/ui/Avatar.vue'
import { useChatsStore } from '@/stores/chats'
import type { Chat } from '@/api/types'

const props = defineProps<{
  chat: Chat
  active: boolean
}>()

const chatsStore = useChatsStore()

const isOnline = computed(() => {
  if (props.chat.type !== 'direct' || !props.chat.other_user_id) return null
  return chatsStore.isUserOnline(props.chat.other_user_id) || null
})

const hasUnread = computed(() => (props.chat.unread_count ?? 0) > 0)

const emit = defineEmits<{
  select: []
  toggleFavorite: []
  togglePin: []
  toggleMute: []
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

/* Swipe handling for mobile */
const swipeX = ref(0)
let startX = 0
let swiping = false
const SWIPE_THRESHOLD = 70

function onTouchStart(e: TouchEvent) {
  startX = e.touches[0]!.clientX
  swiping = true
}

function onTouchMove(e: TouchEvent) {
  if (!swiping) return
  const dx = e.touches[0]!.clientX - startX
  swipeX.value = Math.max(-SWIPE_THRESHOLD, Math.min(SWIPE_THRESHOLD, dx))
}

function onTouchEnd() {
  swiping = false
  swipeX.value = 0
}
</script>

<style scoped>
.chat-item-swipe-wrapper {
  position: relative;
  overflow: hidden;
}

.swipe-actions {
  position: absolute;
  top: 0;
  bottom: 0;
  display: flex;
  align-items: center;
}

.swipe-actions-left {
  left: 0;
}

.swipe-actions-right {
  right: 0;
}

.swipe-action {
  padding: 0 12px;
  height: 100%;
  border: none;
  color: #fff;
  font-size: 0.8rem;
  cursor: pointer;
}

.swipe-pin {
  background: var(--color-primary, #3390ec);
}

.swipe-mute {
  background: var(--color-gray, #888);
}

.chat-item {
  transition: transform 0.15s ease;
  will-change: transform;
}

.chat-item-unread .chat-item-name {
  font-weight: 700;
}

.chat-item-bottom {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 6px;
}

.chat-item-bottom .chat-item-preview {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.unread-badge {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 20px;
  height: 20px;
  padding: 0 6px;
  border-radius: 10px;
  background: var(--color-primary, #3390ec);
  color: #fff;
  font-size: 0.75rem;
  font-weight: 600;
  flex-shrink: 0;
}

.unread-badge-muted {
  background: var(--color-gray, #aaa);
}

.pin-icon {
  font-size: 0.8em;
  margin-right: 2px;
  opacity: 0.6;
}
</style>
