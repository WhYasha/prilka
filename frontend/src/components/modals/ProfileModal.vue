<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal modal-wide">
      <div class="modal-header">
        <h2 class="modal-title">Profile &amp; Settings</h2>
        <button class="icon-btn" @click="emit('close')">&#10005;</button>
      </div>

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

      <div class="modal-body profile-body">
        <!-- Profile tab -->
        <div v-show="activeTab === 'profile'">
          <div class="profile-avatar-section">
            <Avatar
              :name="displayName || authStore.user?.username || '?'"
              :url="authStore.user?.avatar_url"
              size="xl"
            />
            <button class="btn btn-outline btn-sm" @click="fileInput?.click()">
              Change photo
            </button>
            <input ref="fileInput" type="file" accept="image/*" class="hidden" @change="handleAvatarChange" />
            <p class="user-id-display">ID: {{ authStore.user?.id }}</p>
          </div>

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

          <div class="modal-actions">
            <button class="btn btn-primary" @click="saveProfile">Save Profile</button>
            <button class="btn btn-ghost" @click="emit('close')">Cancel</button>
          </div>
        </div>

        <!-- Settings tab -->
        <div v-show="activeTab === 'settings'">
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

          <div class="modal-actions">
            <button class="btn btn-primary" @click="saveSettings">Save Settings</button>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { updateUser, uploadAvatar } from '@/api/users'
import { uploadFile } from '@/api/files'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'

const emit = defineEmits<{ close: [] }>()

const authStore = useAuthStore()
const settingsStore = useSettingsStore()
const { showToast } = useToast()

const activeTab = ref<'profile' | 'settings'>('profile')
const displayName = ref('')
const username = ref('')
const bio = ref('')
const usernameError = ref('')
const fileInput = ref<HTMLInputElement | null>(null)

const theme = ref('light')
const notifications = ref(true)
const language = ref('en')

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
