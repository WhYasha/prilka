<template>
  <div
    class="msg-row"
    :data-message-id="message.id"
    :class="[isMine ? 'mine' : 'theirs', { selected: isSelected }]"
    @contextmenu.prevent="onContextMenu"
    v-bind="longPressHandlers"
    @click="onRowClick"
  >
    <!-- Selection checkbox overlay -->
    <div v-if="selectionStore.selectionMode" class="selection-checkbox">
      <input type="checkbox" :checked="isSelected" tabindex="-1" />
    </div>

    <!-- Sender name for others' messages -->
    <div v-if="!isMine && message.message_type !== 'sticker'" class="msg-sender-name">
      {{ message.sender_display_name || message.sender_username }}
      <Badge v-if="message.sender_is_admin" />
    </div>

    <div class="msg-content-wrap">
      <!-- Reply quote -->
      <div
        v-if="message.reply_to_message_id"
        class="msg-reply-quote"
        @click.stop="emit('replyClick', message.reply_to_message_id!)"
      >
        <div class="msg-reply-quote-bar" />
        <div>
          <div class="msg-reply-quote-name">{{ message.reply_to_sender_name || message.reply_to_sender_username || 'Unknown' }}</div>
          <div class="msg-reply-quote-text">{{ replyQuoteText }}</div>
        </div>
      </div>

      <!-- Sticker -->
      <template v-if="message.message_type === 'sticker'">
        <div class="msg-bubble sticker-bubble">
          <img class="sticker-img" :src="stickerUrl" :alt="message.sticker_label || 'sticker'" />
        </div>
      </template>

      <!-- Voice -->
      <template v-else-if="message.message_type === 'voice'">
        <div class="msg-bubble">
          <div class="voice-player">
            <audio controls :src="voiceSrc" />
            <span v-if="message.duration_seconds" class="voice-dur">
              {{ formatDuration(message.duration_seconds) }}
            </span>
          </div>
        </div>
      </template>

      <!-- Text -->
      <template v-else>
        <div class="msg-bubble" v-html="renderedContent" />
      </template>

      <!-- Hover reaction button -->
      <button class="reaction-add-btn" @click.stop="onReactionBtnClick">&#9786;</button>

      <!-- Reaction chips -->
      <div v-if="message.reactions && message.reactions.length" class="reaction-chips">
        <button
          v-for="r in message.reactions"
          :key="r.emoji"
          class="reaction-chip"
          :class="{ 'reaction-mine': r.me }"
          @click.stop="emit('toggleReaction', message.id, r.emoji)"
        >
          <span class="reaction-emoji">{{ r.emoji }}</span>
          <span class="reaction-count">{{ r.count }}</span>
        </button>
        <button class="reaction-chip reaction-add-chip" @click.stop="onReactionBtnClick">+</button>
      </div>
    </div>

    <div class="msg-time"><span v-if="message.is_edited" class="msg-edited-label">(edited)</span> {{ formatTime(message.created_at) }}</div>
  </div>
</template>

<script setup lang="ts">
import { computed, inject, ref } from 'vue'
import linkifyStr from 'linkify-string'
import Badge from '@/components/ui/Badge.vue'
import type { Message, Sticker } from '@/api/types'
import { useSelectionStore } from '@/stores/selection'
import { useChatsStore } from '@/stores/chats'
import { useLongPress } from '@/composables/useLongPress'

const props = defineProps<{
  message: Message
  isMine: boolean
}>()

const emit = defineEmits<{
  mentionClick: [username: string]
  openEmojiPicker: [messageId: number, x: number, y: number]
  toggleReaction: [messageId: number, emoji: string]
  replyClick: [messageId: number]
}>()

const selectionStore = useSelectionStore()
const chatsStore = useChatsStore()

const isSelected = computed(() => selectionStore.isSelected(props.message.id))

const { onPointerDown, onPointerUp, onPointerMove } = useLongPress({
  delay: 500,
  onLongPress(event: PointerEvent) {
    showContextMenu(event.clientX, event.clientY)
  },
})

const longPressHandlers = {
  onPointerdown: onPointerDown,
  onPointerup: onPointerUp,
  onPointermove: onPointerMove,
}

function onContextMenu(event: MouseEvent) {
  if (selectionStore.selectionMode) {
    selectionStore.toggleMessage(props.message.id)
    return
  }
  showContextMenu(event.clientX, event.clientY)
}

function showContextMenu(x: number, y: number) {
  window.dispatchEvent(
    new CustomEvent('show-message-context-menu', {
      detail: {
        messageId: props.message.id,
        chatId: chatsStore.activeChatId,
        text: props.message.content || '',
        messageType: props.message.message_type,
        senderId: props.message.sender_id,
        senderName: props.message.sender_display_name || props.message.sender_username || '',
        x,
        y,
      },
    }),
  )
}

function onRowClick() {
  if (selectionStore.selectionMode) {
    selectionStore.toggleMessage(props.message.id)
  }
}

const stickers = inject<{ value: Sticker[] }>('stickers', ref([]))

const stickerUrl = computed(() => {
  if (props.message.sticker_url) return props.message.sticker_url
  if (props.message.sticker_id) {
    const local = stickers.value.find((s: Sticker) => s.id === props.message.sticker_id)
    if (local) return local.url
  }
  return ''
})

const voiceSrc = computed(() => {
  return props.message.attachment_url || ''
})

const replyQuoteText = computed(() => {
  const rt = props.message.reply_to_type
  if (rt === 'sticker') return '\u{1F3A8} Sticker'
  if (rt === 'voice') return '\u{1F3A4} Voice message'
  if (rt === 'file') return '\u{1F4CE} File'
  const text = props.message.reply_to_content || ''
  return text.length > 80 ? text.slice(0, 80) + '...' : text
})

const renderedContent = computed(() => {
  const text = linkifyStr(props.message.content || '', {
    defaultProtocol: 'https',
    target: '_blank',
    rel: 'noopener noreferrer',
  })
  // Process @mentions — skip @ signs inside <a ...>...</a> tags
  return text.replace(
    /(<a\s[^>]*>.*?<\/a>)|@(\w{1,30})/g,
    (match, anchorTag, username) => {
      if (anchorTag) return anchorTag
      return `<span class="mention" data-u="${username}" onclick="window.dispatchEvent(new CustomEvent('mention-click', {detail:'${username}'}))">@${username}</span>`
    },
  )
})

function formatTime(isoStr: string): string {
  if (!isoStr) return ''
  const d = new Date(isoStr.endsWith('Z') ? isoStr : isoStr + 'Z')
  const now = new Date()
  if (d.toDateString() === now.toDateString()) {
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
  }
  return d.toLocaleDateString([], { month: 'short', day: 'numeric' })
}

function formatDuration(sec: number): string {
  const m = Math.floor(sec / 60)
  const s = sec % 60
  return `${m}:${String(s).padStart(2, '0')}`
}

function onReactionBtnClick(event: MouseEvent) {
  const rect = (event.target as HTMLElement).getBoundingClientRect()
  emit('openEmojiPicker', props.message.id, rect.left, rect.bottom + 4)
}

// Listen for mention clicks
if (typeof window !== 'undefined') {
  window.addEventListener('mention-click', ((e: CustomEvent) => {
    emit('mentionClick', e.detail)
  }) as EventListener)
}
</script>
