<template>
  <div class="composer-wrap">
    <!-- Reply preview bar -->
    <div v-if="replyTo" class="reply-preview">
      <div class="reply-preview-bar" />
      <div style="flex: 1; min-width: 0;">
        <div class="reply-preview-name">{{ replyTo.sender_display_name || replyTo.sender_username }}</div>
        <div class="reply-preview-text">{{ replyPreviewText }}</div>
      </div>
      <button class="reply-preview-close" aria-label="Cancel reply" @click="emit('cancelReply')">&times;</button>
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
import { ref, computed, onMounted, watch } from 'vue'
import type { Message } from '@/api/types'

const props = defineProps<{
  stickerPickerOpen: boolean
  replyTo?: Message | null
}>()

const emit = defineEmits<{
  send: [content: string]
  toggleStickers: []
  startRecording: []
  typing: []
  cancelReply: []
}>()

const text = ref('')
const inputRef = ref<HTMLTextAreaElement | null>(null)
let typingThrottle = 0

const replyPreviewText = computed(() => {
  if (!props.replyTo) return ''
  const t = props.replyTo.message_type
  if (t === 'sticker') return '\u{1F3A8} Sticker'
  if (t === 'voice') return '\u{1F3A4} Voice message'
  if (t === 'file') return '\u{1F4CE} File'
  const content = props.replyTo.content || ''
  return content.length > 100 ? content.slice(0, 100) + '...' : content
})

function send() {
  const content = text.value.trim()
  if (!content) return
  emit('send', content)
  text.value = ''
  autoResize()
}

function onInput() {
  autoResize()
  // Throttle typing events to once per 2 seconds
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
})

// Focus input when reply is set
watch(() => props.replyTo, (val) => {
  if (val) inputRef.value?.focus()
})

defineExpose({ focus: () => inputRef.value?.focus() })
</script>
