<template>
  <div class="image-message">
    <img
      class="image-message-thumb"
      :src="message.attachment_url"
      :alt="message.attachment_filename || 'image'"
      loading="lazy"
      @click="openFullImage"
      @error="onImageError"
    />
    <div v-if="caption" class="image-message-caption">{{ caption }}</div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue'
import type { Message } from '@/api/types'

const props = defineProps<{
  message: Message
}>()

const caption = computed(() => {
  const content = props.message.content?.trim()
  // Don't show caption if it's empty or same as filename
  if (!content || content === props.message.attachment_filename) return ''
  return content
})

const imageError = ref(false)

function onImageError() {
  imageError.value = true
}

function openFullImage() {
  if (props.message.attachment_url) {
    window.open(props.message.attachment_url, '_blank', 'noopener,noreferrer')
  }
}
</script>

<style scoped>
.image-message {
  display: flex;
  flex-direction: column;
  gap: 4px;
  max-width: 300px;
}

.image-message-thumb {
  max-width: 300px;
  width: 100%;
  height: auto;
  border-radius: 8px;
  cursor: pointer;
  object-fit: contain;
  display: block;
}

.image-message-thumb:hover {
  opacity: 0.9;
}

.image-message-caption {
  font-size: 0.9em;
  color: var(--text-secondary, #666);
  padding: 0 4px;
  word-break: break-word;
}
</style>
