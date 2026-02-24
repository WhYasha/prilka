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
              :name="title || typeLabel"
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
            <label class="form-label">{{ typeLabel }} name</label>
            <input v-model="title" class="form-input" type="text" maxlength="128" :placeholder="`${typeLabel} name`" />
          </div>
          <div class="form-group">
            <label class="form-label">Description</label>
            <textarea v-model="description" class="form-input" maxlength="500" rows="4" :placeholder="`${typeLabel} description...`" />
          </div>
          <div v-if="channelStore.isOwner" class="form-group">
            <label class="form-label">{{ typeLabel }} type</label>
            <div class="type-selector">
              <label class="type-option">
                <input v-model="chatType" type="radio" value="public" name="chatType" />
                <span class="type-option-label">Public</span>
                <span class="type-option-desc">Anyone can find and join</span>
              </label>
              <label class="type-option">
                <input v-model="chatType" type="radio" value="private" name="chatType" />
                <span class="type-option-label">Private</span>
                <span class="type-option-desc">Only invited users can join</span>
              </label>
            </div>
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
import { ref, computed, onMounted } from 'vue'
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
const chatType = ref<'public' | 'private'>('public')
const saving = ref(false)
const fileInput = ref<HTMLInputElement | null>(null)

const isGroup = computed(() => channelStore.detail?.type === 'group')
const typeLabel = computed(() => isGroup.value ? 'Group' : 'Channel')

onMounted(() => {
  if (channelStore.detail) {
    title.value = channelStore.detail.title || ''
    description.value = channelStore.detail.description || ''
    avatarUrl.value = channelStore.detail.avatar_url ?? undefined
    chatType.value = channelStore.detail.public_name ? 'public' : 'private'
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
    showToast(`${typeLabel.value} name is required`)
    return
  }

  saving.value = true
  try {
    await channelStore.updateChannel(props.chatId, {
      title: title.value.trim(),
      description: description.value.trim(),
    })
    showToast(`${typeLabel.value} updated!`)
    emit('saved')
    emit('close')
  } catch {
    showToast(`Failed to update ${typeLabel.value.toLowerCase()}`)
  } finally {
    saving.value = false
  }
}
</script>

<style scoped>
.type-selector {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.type-option {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 12px;
  border-radius: 8px;
  cursor: pointer;
  transition: background var(--transition-fast);
}

.type-option:hover {
  background: var(--input-bg);
}

.type-option-label {
  font-size: 14px;
  font-weight: 500;
  color: var(--text);
}

.type-option-desc {
  font-size: 12px;
  color: var(--text-muted);
  margin-left: auto;
}
</style>
