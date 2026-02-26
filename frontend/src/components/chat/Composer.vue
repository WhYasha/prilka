<template>
  <div class="composer-wrapper">
    <!-- Reply preview bar -->
    <div v-if="replyTarget" class="reply-preview">
      <div class="reply-preview-bar" />
      <div class="reply-preview-content">
        <span class="reply-preview-sender">{{ replyTarget.senderName }}</span>
        <span class="reply-preview-text">{{ replyTarget.snippet }}</span>
      </div>
      <button class="icon-btn reply-close-btn" title="Cancel reply" @click="cancelReply">&#10005;</button>
    </div>

    <!-- Edit preview bar -->
    <div v-if="editTarget" class="reply-preview edit-preview">
      <div class="reply-preview-bar edit-bar" />
      <div class="reply-preview-content">
        <span class="reply-preview-sender">Editing message</span>
        <span class="reply-preview-text">{{ editTarget.snippet }}</span>
      </div>
      <button class="icon-btn reply-close-btn" title="Cancel edit" @click="cancelEdit">&#10005;</button>
    </div>

    <div class="composer">
      <button
        class="icon-btn composer-btn"
        aria-label="Attach file"
        title="Attach file"
        @click="emit('openAttachmentMenu')"
      >
        <Paperclip :size="22" />
      </button>
      <textarea
        ref="inputRef"
        v-model="text"
        class="composer-input"
        placeholder="Message..."
        rows="1"
        maxlength="4000"
        @keydown.enter.exact.prevent="send"
        @keydown.escape="onEscape"
        @input="onInput"
        @contextmenu="onContextMenu"
      />
      <button
        class="icon-btn composer-btn"
        aria-label="Emoji"
        title="Emoji"
        @click="emit('toggleEmojiPanel')"
      >
        <Smile :size="22" />
      </button>
      <button
        v-if="!hasText"
        class="icon-btn composer-btn record-btn"
        aria-label="Voice message"
        title="Record voice"
        @click="emit('startRecording')"
      >
        <Mic :size="22" />
      </button>
      <button
        v-else
        class="icon-btn composer-btn send-btn"
        aria-label="Send"
        title="Send"
        @click="send"
      >
        <SendHorizontal :size="22" />
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch, onMounted, onUnmounted, nextTick } from 'vue'
import { Paperclip, Smile, Mic, SendHorizontal } from 'lucide-vue-next'
import type { Message } from '@/api/types'

export interface ReplyTarget {
  messageId: number
  senderName: string
  snippet: string
}

export interface EditTarget {
  messageId: number
  snippet: string
  fullText: string
}

const props = defineProps<{
  stickerPickerOpen: boolean
  replyTo?: Message | null
}>()

const emit = defineEmits<{
  send: [content: string, replyTo?: { messageId: number; senderName: string; snippet: string }]
  toggleStickers: []
  toggleEmojiPanel: []
  openAttachmentMenu: []
  startRecording: []
  typing: []
  editMessage: [messageId: number, content: string]
}>()

const text = ref('')
const inputRef = ref<HTMLTextAreaElement | null>(null)
const replyTarget = ref<ReplyTarget | null>(null)
const editTarget = ref<EditTarget | null>(null)
let typingThrottle = 0

const hasText = computed(() => text.value.trim().length > 0)

function send() {
  const content = text.value.trim()
  if (!content) return

  if (editTarget.value) {
    emit('editMessage', editTarget.value.messageId, content)
    editTarget.value = null
  } else if (replyTarget.value) {
    emit('send', content, { ...replyTarget.value })
    replyTarget.value = null
  } else {
    emit('send', content)
  }

  text.value = ''
  autoResize()
}

function insertText(str: string) {
  const el = inputRef.value
  if (!el) {
    text.value += str
    return
  }
  const start = el.selectionStart ?? text.value.length
  const end = el.selectionEnd ?? text.value.length
  text.value = text.value.slice(0, start) + str + text.value.slice(end)
  nextTick(() => {
    const pos = start + str.length
    el.selectionStart = pos
    el.selectionEnd = pos
    el.focus()
    autoResize()
  })
}

function cancelReply() {
  replyTarget.value = null
  inputRef.value?.focus()
}

function cancelEdit() {
  editTarget.value = null
  text.value = ''
  autoResize()
  inputRef.value?.focus()
}

function onEscape() {
  if (replyTarget.value) {
    cancelReply()
  } else if (editTarget.value) {
    cancelEdit()
  }
}

function onReplyToMessage(e: Event) {
  const detail = (e as CustomEvent).detail
  if (!detail) return
  editTarget.value = null
  replyTarget.value = {
    messageId: detail.messageId,
    senderName: detail.senderName || 'Unknown',
    snippet: (detail.text || '').slice(0, 100),
  }
  nextTick(() => inputRef.value?.focus())
}

function onEditMessage(e: Event) {
  const detail = (e as CustomEvent).detail
  if (!detail) return
  replyTarget.value = null
  editTarget.value = {
    messageId: detail.messageId,
    snippet: (detail.text || '').slice(0, 100),
    fullText: detail.text || '',
  }
  text.value = editTarget.value.fullText
  nextTick(() => {
    autoResize()
    inputRef.value?.focus()
  })
}

function onContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const el = inputRef.value
  if (!el) return
  if (el.selectionStart !== el.selectionEnd) {
    window.dispatchEvent(new CustomEvent('show-text-format-menu', {
      detail: { x: event.clientX, y: event.clientY, target: el }
    }))
  }
}

function onInput() {
  autoResize()
  const now = Date.now()
  if (now - typingThrottle > 2000 && text.value.trim().length > 0) {
    typingThrottle = now
    emit('typing')
  }
}

function autoResize() {
  const el = inputRef.value
  if (!el) return
  el.style.height = 'auto'
  el.style.height = Math.min(el.scrollHeight, 120) + 'px'
}

onMounted(() => {
  inputRef.value?.focus()
  window.addEventListener('reply-to-message', onReplyToMessage)
  window.addEventListener('edit-message', onEditMessage)
})

onUnmounted(() => {
  window.removeEventListener('reply-to-message', onReplyToMessage)
  window.removeEventListener('edit-message', onEditMessage)
})

// Focus input when reply is set
watch(() => props.replyTo, (val) => {
  if (val) inputRef.value?.focus()
})

defineExpose({ focus: () => inputRef.value?.focus(), insertText })
</script>

<style scoped>
.composer-wrapper {
  display: flex;
  flex-direction: column;
}

.reply-preview {
  display: flex;
  align-items: center;
  padding: 6px 12px;
  background: var(--bg-secondary, #f0f0f0);
  border-top: 1px solid var(--border-color, #e0e0e0);
  gap: 8px;
}

.reply-preview-bar {
  width: 3px;
  min-height: 28px;
  border-radius: 2px;
  background: var(--accent-color, #3390ec);
  flex-shrink: 0;
}

.edit-bar {
  background: var(--edit-color, #e8a53a);
}

.reply-preview-content {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 1px;
}

.reply-preview-sender {
  font-size: 0.8rem;
  font-weight: 600;
  color: var(--accent-color, #3390ec);
}

.edit-preview .reply-preview-sender {
  color: var(--edit-color, #e8a53a);
}

.reply-preview-text {
  font-size: 0.8rem;
  color: var(--text-secondary, #707579);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.reply-close-btn {
  flex-shrink: 0;
  font-size: 0.9rem;
  opacity: 0.6;
}

.reply-close-btn:hover {
  opacity: 1;
}
</style>
