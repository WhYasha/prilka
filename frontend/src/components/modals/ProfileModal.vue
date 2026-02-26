<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal profile-modal--self">
      <!-- View mode: close + edit buttons -->
      <template v-if="mode === 'view'">
        <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
          <X :size="20" :stroke-width="2" />
        </button>
        <button class="icon-btn profile-edit-btn" aria-label="Edit profile" @click="goToEdit">
          <Pencil :size="18" :stroke-width="2" />
        </button>
      </template>

      <Transition :name="transitionName" mode="out-in">
        <!-- VIEW PANEL -->
        <div v-if="mode === 'view'" key="view">
          <!-- Gradient header with avatar -->
          <div class="profile-header">
            <div class="profile-header__gradient" />
            <div class="profile-header__content">
              <Avatar
                :name="authStore.user?.display_name || authStore.user?.username || '?'"
                :url="authStore.user?.avatar_url"
                :online="isOnline"
                size="xxl"
              />
              <div class="profile-header__info">
                <div class="profile-name">
                  {{ authStore.user?.display_name || authStore.user?.username || '?' }}
                  <Badge v-if="authStore.user?.is_admin" />
                </div>
                <div class="profile-status" :class="{ 'profile-status--online': isOnline }">
                  {{ statusText }}
                </div>
              </div>
            </div>
          </div>

          <div class="profile-modal-body">
            <ProfileInfoList
              :username="authStore.user?.username"
              :bio="authStore.user?.bio"
              :user-id="authStore.user?.id"
            />

            <ProfileMediaTabs />
          </div>
        </div>

        <!-- EDIT PANEL -->
        <div v-else key="edit">
          <div class="profile-edit-header">
            <button class="icon-btn" aria-label="Back" @click="goToView">
              <ArrowLeft :size="20" :stroke-width="2" />
            </button>
            <span class="profile-edit-header__title">Edit Profile</span>
          </div>

          <div class="profile-modal-body profile-modal-body--scrollable">
            <!-- Avatar section -->
            <div class="profile-avatar-section profile-avatar-upload-section">
              <div class="profile-avatar-upload-wrap" @click="fileInput?.click()">
                <Avatar
                  :name="displayName || authStore.user?.username || '?'"
                  :url="authStore.user?.avatar_url"
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
                <label class="form-label">Display name</label>
                <input v-model="displayName" class="form-input" type="text" maxlength="64" placeholder="Your name" />
              </div>
              <div class="form-group">
                <label class="form-label">Username</label>
                <input v-model="username" class="form-input" type="text" maxlength="20" placeholder="username" />
                <small v-if="usernameError" class="form-hint form-error">{{ usernameError }}</small>
              </div>
              <div class="form-group">
                <label class="form-label">Bio</label>
                <textarea v-model="bio" class="form-input" maxlength="200" rows="3" placeholder="About you..." />
              </div>
            </div>

            <div class="profile-edit-actions">
              <button class="btn btn-primary" @click="saveProfile">Save Profile</button>
              <button class="btn btn-ghost" @click="goToView">Cancel</button>
            </div>
          </div>
        </div>
      </Transition>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useChatsStore } from '@/stores/chats'
import { updateUser, uploadAvatar } from '@/api/users'
import { uploadFile } from '@/api/files'
import { useToast } from '@/composables/useToast'
import { formatLastSeen } from '@/utils/formatLastSeen'
import Avatar from '@/components/ui/Avatar.vue'
import Badge from '@/components/ui/Badge.vue'
import ProfileInfoList from '@/components/profile/ProfileInfoList.vue'
import ProfileMediaTabs from '@/components/profile/ProfileMediaTabs.vue'
import { X, Camera, Pencil, ArrowLeft } from 'lucide-vue-next'

const emit = defineEmits<{ close: [] }>()

const authStore = useAuthStore()
const chatsStore = useChatsStore()
const { showToast } = useToast()

const mode = ref<'view' | 'edit'>('view')
const transitionName = ref('profile-slide-left')

const displayName = ref('')
const username = ref('')
const bio = ref('')
const usernameError = ref('')
const fileInput = ref<HTMLInputElement | null>(null)

const isOnline = computed(() => {
  if (!authStore.user) return false
  return chatsStore.isUserOnline(authStore.user.id)
})

const statusText = computed(() => {
  if (isOnline.value) return 'online'
  if (!authStore.user) return 'offline'
  const presence = chatsStore.getUserPresence(authStore.user.id)
  if (presence?.lastSeenAt) return formatLastSeen(presence.lastSeenAt)
  if (presence?.lastSeenBucket) return `last seen ${presence.lastSeenBucket}`
  return 'offline'
})

function resetFormFields() {
  if (authStore.user) {
    displayName.value = authStore.user.display_name || ''
    username.value = authStore.user.username
    bio.value = authStore.user.bio || ''
  }
  usernameError.value = ''
}

function goToEdit() {
  resetFormFields()
  transitionName.value = 'profile-slide-left'
  mode.value = 'edit'
}

function goToView() {
  transitionName.value = 'profile-slide-right'
  mode.value = 'view'
}

async function handleAvatarChange() {
  const file = fileInput.value?.files?.[0]
  if (!file) return
  try {
    const fileData = await uploadFile(file)
    const result = await uploadAvatar(fileData.id)
    authStore.updateUser({ avatar_url: result.avatar_url })
    showToast('Avatar updated!')
  } catch {
    showToast('Upload failed')
  }
  if (fileInput.value) fileInput.value.value = ''
}

async function saveProfile() {
  usernameError.value = ''
  if (!authStore.user) return

  const payload: { display_name?: string; bio?: string; username?: string } = {
    display_name: displayName.value.trim(),
    bio: bio.value.trim(),
  }
  if (username.value.trim() && username.value.trim() !== authStore.user.username) {
    payload.username = username.value.trim()
  }

  try {
    const updated = await updateUser(authStore.user.id, payload)
    authStore.updateUser({
      display_name: updated.display_name,
      username: updated.username,
    })
    showToast('Profile saved!')
    goToView()
  } catch (e: unknown) {
    const err = e as { response?: { status?: number } }
    if (err.response?.status === 409) {
      usernameError.value = 'Username already taken'
    } else {
      showToast('Save failed')
    }
  }
}
</script>

<style scoped>
.profile-header {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 0 0 1.25rem;
}

.profile-header__gradient {
  position: absolute;
  inset: 0;
  bottom: 40%;
  background: linear-gradient(135deg, var(--accent) 0%, color-mix(in srgb, var(--accent) 60%, #6c5ce7) 100%);
  border-radius: var(--modal-radius) var(--modal-radius) 0 0;
}

.profile-header__content {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  padding-top: 2rem;
}

.profile-header__info {
  display: flex;
  flex-direction: column;
  align-items: center;
  margin-top: .75rem;
}

.profile-edit-btn {
  position: absolute;
  top: 12px;
  right: 48px;
  z-index: 2;
}

.profile-edit-header {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 12px 16px;
  border-bottom: 1px solid var(--profile-divider);
}

.profile-edit-header__title {
  font-size: 1rem;
  font-weight: 600;
  color: var(--text);
}

/* Slide transitions */
.profile-slide-left-enter-active,
.profile-slide-left-leave-active,
.profile-slide-right-enter-active,
.profile-slide-right-leave-active {
  transition: transform 0.25s ease, opacity 0.25s ease;
}

.profile-slide-left-enter-from {
  transform: translateX(30px);
  opacity: 0;
}

.profile-slide-left-leave-to {
  transform: translateX(-30px);
  opacity: 0;
}

.profile-slide-right-enter-from {
  transform: translateX(-30px);
  opacity: 0;
}

.profile-slide-right-leave-to {
  transform: translateX(30px);
  opacity: 0;
}
</style>
