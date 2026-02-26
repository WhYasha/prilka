<template>
  <div
    class="msg-row"
    :data-message-id="message.id"
    :class="[isMine ? 'mine' : 'theirs', { selected: isSelected, 'msg-deleting': isBeingDeleted, 'msg-new': isFreshSend }]"
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
      <!-- Sticker -->
      <template v-if="message.message_type === 'sticker'">
        <div class="msg-bubble sticker-bubble">
          <img class="sticker-img" :src="stickerUrl" :alt="message.sticker_label || 'sticker'" />
          <div class="msg-time"><span v-if="message.is_edited" class="msg-edited-label">(edited)</span> {{ formatTime(message.created_at) }}<span v-if="isMine && readReceiptIndicator" class="msg-read-check" :class="readReceiptIndicator.cls">{{ readReceiptIndicator.text }}</span></div>
        </div>
      </template>

      <!-- Voice -->
      <template v-else-if="message.message_type === 'voice'">
        <div class="msg-bubble">
          <!-- Forwarded attribution -->
          <div v-if="forwardedLabel" class="msg-forwarded-label">
            {{ forwardedLabel }}
          </div>
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
          <div class="voice-player">
            <button class="voice-play-btn" @click.stop="toggleVoice">
              {{ voicePlaying ? '\u23F8' : '\u25B6' }}
            </button>
            <div class="voice-track" @click.stop="seekVoice">
              <div class="voice-progress" :style="{ width: voiceProgress + '%' }" />
            </div>
            <span class="voice-dur">{{ voiceTimeDisplay }}</span>
          </div>
          <div class="msg-time"><span v-if="message.is_edited" class="msg-edited-label">(edited)</span> {{ formatTime(message.created_at) }}<span v-if="isMine && readReceiptIndicator" class="msg-read-check" :class="readReceiptIndicator.cls">{{ readReceiptIndicator.text }}</span></div>
        </div>
      </template>

      <!-- Text -->
      <template v-else>
        <div class="msg-bubble">
          <!-- Forwarded attribution -->
          <div v-if="forwardedLabel" class="msg-forwarded-label">
            {{ forwardedLabel }}
          </div>
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
          <span class="msg-text" v-html="renderedContent" />
          <div class="msg-time"><span v-if="message.is_edited" class="msg-edited-label">(edited)</span> {{ formatTime(message.created_at) }}<span v-if="isMine && readReceiptIndicator" class="msg-read-check" :class="readReceiptIndicator.cls">{{ readReceiptIndicator.text }}</span></div>
        </div>
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
  </div>
</template>

<script setup lang="ts">
import { computed, inject, ref, onUnmounted, watch } from 'vue'
import linkifyStr from 'linkify-string'
import Badge from '@/components/ui/Badge.vue'
import type { Message, Sticker } from '@/api/types'
import { useSelectionStore } from '@/stores/selection'
import { useChatsStore } from '@/stores/chats'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { useLongPress } from '@/composables/useLongPress'
import { useMessagesStore } from '@/stores/messages'

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
const messagesStore = useMessagesStore()

const isSelected = computed(() => selectionStore.isSelected(props.message.id))
const isBeingDeleted = computed(() => messagesStore.deletingMessages.has(props.message.id))

const lastSentMessageId = inject<{ value: number | null }>('lastSentMessageId', ref(null))
const isFreshSend = computed(() => props.isMine && props.message.id === lastSentMessageId.value)

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

const authStore = useAuthStore()
const settingsStore = useSettingsStore()

const readReceiptIndicator = computed(() => {
  if (!settingsStore.readReceiptsEnabled) return null
  const chat = chatsStore.activeChat
  if (!chat) return null
  const chatId = chat.id

  // Self-chat (Saved Messages): always double-check
  if (chat.type === 'direct' && chat.other_user_id === authStore.user?.id) {
    return { text: '\u2713\u2713', cls: 'read' }
  }

  if (chat.type === 'direct') {
    const isRead = messagesStore.isMessageRead(chatId, props.message.id, chat.other_user_id)
    return isRead
      ? { text: '\u2713\u2713', cls: 'read' }
      : { text: '\u2713', cls: 'sent' }
  }

  // Group/channel: show read count
  const count = messagesStore.getReadCount(chatId, props.message.id)
  if (count > 0) {
    return { text: `\u2713\u2713 ${count}`, cls: 'read' }
  }
  return { text: '\u2713', cls: 'sent' }
})

const forwardedLabel = computed(() => {
  if (!props.message.forwarded_from_chat_id) return null
  const origUserId = props.message.forwarded_from_user_id
  // If the original sender is myself, don't show "Forwarded from" — it's just my message
  if (origUserId && origUserId === authStore.user?.id) return null
  const name = props.message.forwarded_from_display_name
  if (name) return `Forwarded from ${name}`
  return 'Forwarded'
})

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

// Voice player state
const voicePlaying = ref(false)
const voiceProgress = ref(0)
const voiceCurrentTime = ref(0)
const voiceDuration = ref(props.message.duration_seconds || 0)
let voiceAudio: HTMLAudioElement | null = null
let voiceRafId: number | null = null

function getVoiceAudio(): HTMLAudioElement {
  if (!voiceAudio) {
    voiceAudio = new Audio(voiceSrc.value)
    voiceAudio.addEventListener('loadedmetadata', () => {
      if (voiceAudio && isFinite(voiceAudio.duration)) {
        voiceDuration.value = voiceAudio.duration
      }
    })
    voiceAudio.addEventListener('ended', () => {
      voicePlaying.value = false
      voiceProgress.value = 0
      voiceCurrentTime.value = 0
      cancelVoiceRaf()
    })
  }
  return voiceAudio
}

function updateVoiceProgress() {
  if (voiceAudio && voicePlaying.value) {
    voiceCurrentTime.value = voiceAudio.currentTime
    const dur = voiceAudio.duration
    voiceProgress.value = dur && isFinite(dur) ? (voiceAudio.currentTime / dur) * 100 : 0
    voiceRafId = requestAnimationFrame(updateVoiceProgress)
  }
}

function cancelVoiceRaf() {
  if (voiceRafId !== null) {
    cancelAnimationFrame(voiceRafId)
    voiceRafId = null
  }
}

function toggleVoice() {
  const audio = getVoiceAudio()
  if (voicePlaying.value) {
    audio.pause()
    voicePlaying.value = false
    cancelVoiceRaf()
  } else {
    audio.play()
    voicePlaying.value = true
    voiceRafId = requestAnimationFrame(updateVoiceProgress)
  }
}

function seekVoice(e: MouseEvent) {
  const audio = getVoiceAudio()
  const target = e.currentTarget as HTMLElement
  const rect = target.getBoundingClientRect()
  const ratio = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width))
  const dur = audio.duration
  if (dur && isFinite(dur)) {
    audio.currentTime = ratio * dur
    voiceCurrentTime.value = audio.currentTime
    voiceProgress.value = ratio * 100
  }
}

const voiceTimeDisplay = computed(() => {
  const dur = voiceDuration.value
  if (voicePlaying.value || voiceCurrentTime.value > 0) {
    return formatDuration(Math.floor(voiceCurrentTime.value)) + ' / ' + formatDuration(Math.floor(dur))
  }
  return formatDuration(Math.floor(dur))
})

// Update audio src if attachment_url changes
watch(voiceSrc, () => {
  if (voiceAudio) {
    voiceAudio.pause()
    voicePlaying.value = false
    cancelVoiceRaf()
    voiceAudio = null
    voiceProgress.value = 0
    voiceCurrentTime.value = 0
  }
})

onUnmounted(() => {
  if (voiceAudio) {
    voiceAudio.pause()
    voiceAudio = null
  }
  cancelVoiceRaf()
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
    rel: 'noopener noreferrer',
  })
  // Process @mentions — skip @ signs inside <a ...>...</a> tags
  return text.replace(
    /(<a\s[^>]*>.*?<\/a>)|@(\w{1,30})/g,
    (_match: string, anchorTag: string, username: string) => {
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
