<template>
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
      @input="autoResize"
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
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'

defineProps<{
  stickerPickerOpen: boolean
}>()

const emit = defineEmits<{
  send: [content: string]
  toggleStickers: []
  startRecording: []
}>()

const text = ref('')
const inputRef = ref<HTMLTextAreaElement | null>(null)

function send() {
  const content = text.value.trim()
  if (!content) return
  emit('send', content)
  text.value = ''
  autoResize()
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

defineExpose({ focus: () => inputRef.value?.focus() })
</script>
