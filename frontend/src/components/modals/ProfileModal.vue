<template>
  <div class="modal-backdrop profile-backdrop" @click.self="emit('close')">
    <div class="modal profile-modal profile-modal--self">
      <button class="icon-btn profile-close-btn" aria-label="Close" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>

      <!-- Tabs -->
      <div class="profile-tabs">
        <button
          class="profile-tab"
          :class="{ active: activeTab === 'profile' }"
          @click="activeTab = 'profile'"
        >Profile</button>
        <button
          class="profile-tab"
          :class="{ active: activeTab === 'settings' }"
          @click="activeTab = 'settings'"
        >Settings</button>
      </div>

      <div class="profile-modal-body profile-modal-body--scrollable">
        <!-- Profile tab -->
        <div v-show="activeTab === 'profile'">
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

          <!-- Name display -->
          <div class="profile-name">
            {{ displayName || authStore.user?.username || '?' }}
          </div>
          <div class="profile-status profile-status--online" v-if="authStore.user">
            online
          </div>

          <!-- Divider -->
          <div class="profile-divider" />

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

            <div class="profile-info-item">
              <div class="profile-info-label">User ID</div>
              <div class="profile-info-value">{{ authStore.user?.id }}</div>
            </div>
          </div>

          <div class="profile-edit-actions">
            <button class="btn btn-primary" @click="saveProfile">Save Profile</button>
            <button class="btn btn-ghost" @click="emit('close')">Cancel</button>
          </div>
        </div>

        <!-- Settings tab -->
        <div v-show="activeTab === 'settings'">
          <div class="profile-info-list">
            <div class="form-group form-inline">
              <label class="form-label">Theme</label>
              <select v-model="theme" class="form-input form-select">
                <option value="light">Light</option>
                <option value="dark">Dark</option>
              </select>
            </div>
            <div class="form-group form-inline">
              <label class="form-label">Notifications</label>
              <label class="toggle">
                <input v-model="notifications" type="checkbox" />
                <span class="toggle-slider" />
              </label>
            </div>
            <div class="form-group form-inline">
              <label class="form-label">Language</label>
              <select v-model="language" class="form-input form-select">
                <option value="en">English</option>
                <option value="ru">&#1056;&#1091;&#1089;&#1089;&#1082;&#1080;&#1081;</option>
                <option value="de">Deutsch</option>
              </select>
            </div>
          </div>

          <div class="profile-edit-actions">
            <button class="btn btn-primary" @click="saveSettings">Save Settings</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, watch, onMounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { updateUser, uploadAvatar } from '@/api/users'
import { uploadFile } from '@/api/files'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import { X, Camera } from 'lucide-vue-next'

const props = withDefaults(defineProps<{ initialTab?: 'profile' | 'settings' }>(), {
  initialTab: 'profile',
})

const emit = defineEmits<{ close: [] }>()

const authStore = useAuthStore()
const settingsStore = useSettingsStore()
const { showToast } = useToast()

const activeTab = ref<'profile' | 'settings'>(props.initialTab)
const displayName = ref('')
const username = ref('')
const bio = ref('')
const usernameError = ref('')
const fileInput = ref<HTMLInputElement | null>(null)

const theme = ref('light')
const notifications = ref(true)
const language = ref('en')

watch(notifications, async (enabled) => {
  if (enabled && 'Notification' in window) {
    const permission = await Notification.requestPermission()
    if (permission === 'denied') {
      notifications.value = false
      showToast('Notifications blocked by browser')
    }
  }
})

onMounted(async () => {
  if (authStore.user) {
    displayName.value = authStore.user.display_name || ''
    username.value = authStore.user.username
    bio.value = authStore.user.bio || ''
  }
  theme.value = settingsStore.theme
  notifications.value = settingsStore.notificationsEnabled
  language.value = settingsStore.language
})

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
    emit('close')
  } catch (e: unknown) {
    const err = e as { response?: { status?: number } }
    if (err.response?.status === 409) {
      usernameError.value = 'Username already taken'
    } else {
      showToast('Save failed')
    }
  }
}

async function saveSettings() {
  try {
    await settingsStore.saveSettings({
      theme: theme.value,
      notifications_enabled: notifications.value,
      language: language.value,
    })
    showToast('Settings saved!')
    emit('close')
  } catch {
    showToast('Settings save failed')
  }
}
</script>
