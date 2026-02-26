<template>
  <div class="file-message">
    <a
      class="file-message-card"
      :href="message.attachment_url"
      target="_blank"
      rel="noopener noreferrer"
      :download="message.attachment_filename"
    >
      <div class="file-message-icon">&#128196;</div>
      <div class="file-message-info">
        <div class="file-message-name">{{ displayFilename }}</div>
        <div v-if="caption" class="file-message-caption">{{ caption }}</div>
      </div>
    </a>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { Message } from '@/api/types'

const props = defineProps<{
  message: Message
}>()

const displayFilename = computed(() => {
  return props.message.attachment_filename || 'File'
})

const caption = computed(() => {
  const content = props.message.content?.trim()
  if (!content || content === props.message.attachment_filename) return ''
  return content
})
</script>

<style scoped>
.file-message {
  display: flex;
  flex-direction: column;
  gap: 4px;
  max-width: 300px;
}

.file-message-card {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 14px;
  border-radius: 8px;
  background: var(--bg-tertiary, rgba(0, 0, 0, 0.05));
  text-decoration: none;
  color: inherit;
  cursor: pointer;
  transition: background 0.15s;
}

.file-message-card:hover {
  background: var(--bg-quaternary, rgba(0, 0, 0, 0.08));
}

.file-message-icon {
  font-size: 1.8em;
  flex-shrink: 0;
  line-height: 1;
}

.file-message-info {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.file-message-name {
  font-size: 0.9em;
  font-weight: 500;
  word-break: break-all;
  color: var(--text-primary, #222);
}

.file-message-caption {
  font-size: 0.8em;
  color: var(--text-secondary, #666);
  word-break: break-word;
}
</style>
