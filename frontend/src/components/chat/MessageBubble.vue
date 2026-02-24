<template>
  <div class="msg-row" :class="isMine ? 'mine' : 'theirs'">
    <!-- Sender name for others' messages -->
    <div v-if="!isMine && message.message_type !== 'sticker'" class="msg-sender-name">
      {{ message.sender_display_name || message.sender_username }}
      <Badge v-if="message.sender_is_admin" />
    </div>

    <div class="msg-content-wrap">
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

    <div class="msg-time">{{ formatTime(message.created_at) }}</div>
  </div>
</template>

<script setup lang="ts">
import { computed, inject, ref } from 'vue'
import Badge from '@/components/ui/Badge.vue'
import type { Message, Sticker } from '@/api/types'

const props = defineProps<{
  message: Message
  isMine: boolean
}>()

const emit = defineEmits<{
  mentionClick: [username: string]
  openEmojiPicker: [messageId: number, x: number, y: number]
  toggleReaction: [messageId: number, emoji: string]
}>()

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

const renderedContent = computed(() => {
  const text = escHtml(props.message.content || '')
  // Process @mentions
  return text.replace(
    /@(\w{1,30})/g,
    '<span class="mention" data-u="$1" onclick="window.dispatchEvent(new CustomEvent(\'mention-click\', {detail:\'$1\'}))">@$1</span>',
  )
})

function escHtml(str: string): string {
  return str
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
}

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
