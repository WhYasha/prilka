<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal">
      <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>

      <div class="profile-modal-body profile-modal-body--scrollable">
        <!-- Avatar section -->
        <div class="profile-avatar-section profile-avatar-upload-section">
          <div class="profile-avatar-upload-wrap" @click="fileInput?.click()">
            <Avatar
              :name="title || 'Channel'"
              :url="avatarUrl"
              size="xxl"
            />
            <div class="profile-avatar-overlay">
              <Camera :size="24" :stroke-width="1.8" class="profile-avatar-overlay-icon" />
            </div>
          </div>
          <input ref="fileInput" type="file" accept="image/*" class="hidden" @change="handleAvatarChange" />
        </div>

        <!-- Edit form -->
        <div class="profile-info-list">
          <div class="form-group">
            <label class="form-label">Channel name</label>
            <input v-model="title" class="form-input" type="text" maxlength="128" placeholder="Channel name" />
          </div>
          <div class="form-group">
            <label class="form-label">Description</label>
            <textarea v-model="description" class="form-input" maxlength="500" rows="4" placeholder="Channel description..." />
          </div>
        </div>

        <div class="profile-edit-actions">
          <button class="btn btn-primary" :disabled="saving" @click="save">
            {{ saving ? 'Saving...' : 'Save' }}
          </button>
          <button class="btn btn-ghost" @click="emit('close')">Cancel</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useChannelStore } from '@/stores/channel'
import { uploadChatAvatar } from '@/api/channel'
import { uploadFile } from '@/api/files'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import { X, Camera } from 'lucide-vue-next'

const props = defineProps<{ chatId: number }>()

const emit = defineEmits<{
  close: []
  saved: []
}>()

const channelStore = useChannelStore()
const { showToast } = useToast()

const title = ref('')
const description = ref('')
const avatarUrl = ref<string | undefined>(undefined)
const saving = ref(false)
const fileInput = ref<HTMLInputElement | null>(null)

onMounted(() => {
  if (channelStore.detail) {
    title.value = channelStore.detail.title || ''
    description.value = channelStore.detail.description || ''
    avatarUrl.value = channelStore.detail.avatar_url
  }
})

async function handleAvatarChange() {
  const file = fileInput.value?.files?.[0]
  if (!file) return
  try {
    const fileData = await uploadFile(file)
    await uploadChatAvatar(props.chatId, fileData.id)
    avatarUrl.value = URL.createObjectURL(file)
    showToast('Avatar updated!')
  } catch {
    showToast('Avatar upload failed')
  }
  if (fileInput.value) fileInput.value.value = ''
}

async function save() {
  if (!title.value.trim()) {
    showToast('Channel name is required')
    return
  }

  saving.value = true
  try {
    await channelStore.updateChannel(props.chatId, {
      title: title.value.trim(),
      description: description.value.trim(),
    })
    showToast('Channel updated!')
    emit('saved')
    emit('close')
  } catch {
    showToast('Failed to update channel')
  } finally {
    saving.value = false
  }
}
</script>
