<template>
  <div
    v-if="visible"
    class="attachment-menu"
    @click.stop
  >
    <button class="attachment-option" @click="pickFile('media')">
      <Image :size="20" />
      <span>Photo or Video</span>
    </button>
    <button class="attachment-option" @click="pickFile('document')">
      <FileText :size="20" />
      <span>Document</span>
    </button>
    <input
      ref="fileInput"
      type="file"
      hidden
      :accept="fileAccept"
      @change="onFileChange"
    />
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { Image, FileText } from 'lucide-vue-next'

defineProps<{
  visible: boolean
}>()

const emit = defineEmits<{
  'file-selected': [file: File, type: 'media' | 'document']
  close: []
}>()

const fileInput = ref<HTMLInputElement | null>(null)
const fileAccept = ref('')
const currentType = ref<'media' | 'document'>('media')

function pickFile(type: 'media' | 'document') {
  currentType.value = type
  fileAccept.value = type === 'media' ? 'image/*,video/*' : '*/*'
  // Wait for accept to update before opening
  setTimeout(() => {
    fileInput.value?.click()
  }, 0)
}

function onFileChange(e: Event) {
  const input = e.target as HTMLInputElement
  const file = input.files?.[0]
  if (file) {
    emit('file-selected', file, currentType.value)
  }
  // Reset so the same file can be selected again
  input.value = ''
  emit('close')
}
</script>

<style scoped>
.attachment-menu {
  position: absolute;
  bottom: 100%;
  left: 8px;
  margin-bottom: 8px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 12px;
  padding: 4px;
  display: flex;
  flex-direction: column;
  min-width: 180px;
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.12);
  z-index: 100;
}

.attachment-option {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 10px 12px;
  border: none;
  background: none;
  border-radius: 8px;
  cursor: pointer;
  font-size: 0.95rem;
  color: var(--text);
  transition: background 0.15s;
}

.attachment-option:hover {
  background: var(--hover);
}
</style>
