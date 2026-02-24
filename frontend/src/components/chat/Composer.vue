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
        aria-label="Stickers"
        title="Stickers"
        @click="emit('toggleStickers')"
      >&#127773;</button>
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
      />
      <button
        class="icon-btn composer-btn record-btn"
        aria-label="Voice message"
        title="Record voice"
        @click="emit('startRecording')"
      >&#127908;</button>
      <button
        class="icon-btn composer-btn send-btn"
        aria-label="Send"
        title="Send"
        @click="send"
      >&#10148;</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, nextTick } from 'vue'

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

defineProps<{
  stickerPickerOpen: boolean
}>()

const emit = defineEmits<{
  send: [content: string, replyTo?: { messageId: number; senderName: string; snippet: string }]
  toggleStickers: []
  startRecording: []
  typing: []
  editMessage: [messageId: number, content: string]
}>()

const text = ref('')
const inputRef = ref<HTMLTextAreaElement | null>(null)
const replyTarget = ref<ReplyTarget | null>(null)
const editTarget = ref<EditTarget | null>(null)
let typingThrottle = 0

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

defineExpose({ focus: () => inputRef.value?.focus() })
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
