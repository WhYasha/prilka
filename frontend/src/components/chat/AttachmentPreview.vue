<template>
  <div class="modal-backdrop" @click.self="emit('cancel')">
    <div class="modal">
      <div class="modal-header">
        <h2 class="modal-title">{{ modalTitle }}</h2>
        <button class="icon-btn" @click="emit('cancel')">&#10005;</button>
      </div>
      <div class="modal-body">
        <div v-if="error" class="attachment-error">{{ error }}</div>
        <template v-else>
          <div class="attachment-preview-area">
            <img
              v-if="isImage"
              :src="previewUrl"
              :alt="file.name"
              class="attachment-thumbnail"
            />
            <div v-else class="attachment-file-icon">
              <span class="file-icon">📄</span>
              <span class="file-name">{{ file.name }}</span>
              <span class="file-size">{{ formattedSize }}</span>
            </div>
          </div>
          <input
            v-model="caption"
            class="caption-input"
            type="text"
            placeholder="Add a caption..."
            @keydown.enter="handleSend"
          />
        </template>
      </div>
      <div class="modal-actions">
        <button class="btn btn-ghost" @click="emit('cancel')">Cancel</button>
        <button class="btn btn-primary" :disabled="!!error" @click="handleSend">Send</button>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'

const MAX_FILE_SIZE = 50 * 1024 * 1024 // 50MB

const props = defineProps<{
  file: File
}>()

const emit = defineEmits<{
  send: [file: File, caption: string]
  cancel: []
}>()

const caption = ref('')
const previewUrl = ref('')

const isImage = computed(() => props.file.type.startsWith('image/'))

const modalTitle = computed(() => {
  if (isImage.value) return 'Send Image'
  if (props.file.type.startsWith('video/')) return 'Send Video'
  if (props.file.type.startsWith('audio/')) return 'Send Audio'
  return 'Send File'
})

const error = computed(() => {
  if (props.file.size > MAX_FILE_SIZE) {
    return 'File is too large. Maximum size is 50MB.'
  }
  return null
})

const formattedSize = computed(() => {
  const bytes = props.file.size
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
})

function handleSend() {
  if (error.value) return
  emit('send', props.file, caption.value)
}

onMounted(() => {
  if (isImage.value) {
    previewUrl.value = URL.createObjectURL(props.file)
  }
})

onUnmounted(() => {
  if (previewUrl.value) {
    URL.revokeObjectURL(previewUrl.value)
  }
})
</script>

<style scoped>
.attachment-preview-area {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 120px;
  margin-bottom: 12px;
}

.attachment-thumbnail {
  max-width: 100%;
  max-height: 300px;
  border-radius: 8px;
  object-fit: contain;
}

.attachment-file-icon {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  padding: 24px;
}

.file-icon {
  font-size: 3rem;
}

.file-name {
  font-weight: 500;
  word-break: break-all;
  text-align: center;
  max-width: 300px;
}

.file-size {
  font-size: 0.8rem;
  color: var(--color-text-secondary, #888);
}

.caption-input {
  width: 100%;
  padding: 10px 12px;
  border: 1px solid var(--color-border, #ddd);
  border-radius: 8px;
  font-size: 0.95rem;
  background: var(--color-bg-input, #fff);
  color: var(--text-primary, #111);
  outline: none;
  box-sizing: border-box;
}

.caption-input:focus {
  border-color: var(--color-primary, #3a82f6);
}

.attachment-error {
  color: var(--color-danger, #e53935);
  text-align: center;
  padding: 24px;
  font-size: 0.95rem;
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.5rem;
  padding: 1rem;
}
</style>
