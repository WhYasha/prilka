<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal modal-wide settings-modal">
      <!-- Header bar -->
      <div class="modal-header">
        <button class="icon-btn" aria-label="Back" @click="emit('close')">
          <ArrowLeft :size="20" :stroke-width="2" />
        </button>
        <span class="modal-title">Settings</span>
        <button class="icon-btn" aria-label="Close" @click="emit('close')">
          <X :size="20" :stroke-width="2" />
        </button>
      </div>

      <div class="modal-body">
        <!-- Profile header -->
        <div class="settings-profile-header">
          <Avatar
            :name="authStore.user?.display_name || authStore.user?.username || '?'"
            :url="authStore.user?.avatar_url"
            size="xl"
          />
          <div class="settings-profile-info">
            <div class="settings-display-name">
              {{ authStore.user?.display_name || authStore.user?.username || '---' }}
            </div>
            <div class="settings-username">@{{ authStore.user?.username || '---' }}</div>
          </div>
        </div>

        <div class="settings-divider" />

        <!-- Appearance -->
        <div class="settings-group">
          <div class="menu-item menu-item--setting">
            <Palette class="menu-item__icon" :size="24" :stroke-width="1.8" />
            <span class="menu-item__label">Theme</span>
            <select v-model="theme" class="form-input form-select settings-select">
              <option value="light">Light</option>
              <option value="dark">Dark</option>
            </select>
          </div>
        </div>

        <div class="settings-divider" />

        <!-- Night Mode toggle (Telegram-style) -->
        <div class="settings-group">
          <div class="menu-item menu-item--toggle" @click="toggleNightMode">
            <Moon class="menu-item__icon" :size="24" :stroke-width="1.8" />
            <span class="menu-item__label">Night Mode</span>
            <span
              class="toggle-switch"
              :class="{ 'toggle-switch--on': theme === 'dark' }"
              role="switch"
              :aria-checked="theme === 'dark'"
            >
              <span class="toggle-switch__thumb" />
            </span>
          </div>
        </div>

        <div class="settings-divider" />

        <!-- Notifications -->
        <div class="settings-group">
          <div class="menu-item menu-item--toggle" @click="notifications = !notifications">
            <Bell class="menu-item__icon" :size="24" :stroke-width="1.8" />
            <span class="menu-item__label">Notifications</span>
            <span
              class="toggle-switch"
              :class="{ 'toggle-switch--on': notifications }"
              role="switch"
              :aria-checked="notifications"
            >
              <span class="toggle-switch__thumb" />
            </span>
          </div>
        </div>

        <div class="settings-divider" />

        <!-- Language -->
        <div class="settings-group">
          <div class="menu-item menu-item--setting">
            <Globe class="menu-item__icon" :size="24" :stroke-width="1.8" />
            <span class="menu-item__label">Language</span>
            <select v-model="language" class="form-input form-select settings-select">
              <option value="en">English</option>
              <option value="ru">Русский</option>
              <option value="de">Deutsch</option>
            </select>
          </div>
        </div>

        <div class="settings-divider" />

        <!-- Privacy -->
        <div class="settings-group">
          <h4 class="form-section-heading">Privacy</h4>
          <div class="menu-item menu-item--setting">
            <component
              :is="lastSeenVisibility === 'nobody' ? EyeOff : Eye"
              class="menu-item__icon"
              :size="24"
              :stroke-width="1.8"
            />
            <span class="menu-item__label">Last Seen &amp; Online</span>
            <select v-model="lastSeenVisibility" class="form-input form-select settings-select">
              <option value="everyone">Everyone</option>
              <option value="approx_only">Approximate only</option>
              <option value="nobody">Nobody</option>
            </select>
          </div>
        </div>

        <div class="settings-divider" />

        <div class="settings-actions">
          <button class="btn btn-primary" @click="saveSettings">Save Settings</button>
          <button class="btn btn-ghost" @click="emit('close')">Cancel</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, watch, onMounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { useToast } from '@/composables/useToast'
import Avatar from '@/components/ui/Avatar.vue'
import {
  X,
  ArrowLeft,
  Palette,
  Bell,
  Globe,
  Eye,
  EyeOff,
  Moon,
} from 'lucide-vue-next'

const emit = defineEmits<{ close: [] }>()

const authStore = useAuthStore()
const settingsStore = useSettingsStore()
const { showToast } = useToast()

const theme = ref('light')
const notifications = ref(true)
const language = ref('en')
const lastSeenVisibility = ref<'everyone' | 'approx_only' | 'nobody'>('everyone')

watch(notifications, async (enabled) => {
  if (enabled && 'Notification' in window) {
    const permission = await Notification.requestPermission()
    if (permission === 'denied') {
      notifications.value = false
      showToast('Notifications blocked by browser')
    }
  }
})

onMounted(() => {
  theme.value = settingsStore.theme
  notifications.value = settingsStore.notificationsEnabled
  language.value = settingsStore.language
  lastSeenVisibility.value = settingsStore.lastSeenVisibility
})

function toggleNightMode() {
  theme.value = theme.value === 'dark' ? 'light' : 'dark'
}

async function saveSettings() {
  try {
    await settingsStore.saveSettings({
      theme: theme.value,
      notifications_enabled: notifications.value,
      language: language.value,
      last_seen_visibility: lastSeenVisibility.value,
    })
    showToast('Settings saved')
    emit('close')
  } catch {
    showToast('Failed to save settings')
  }
}
</script>

<style scoped>
.settings-modal {
  max-width: 420px;
}

.settings-profile-header {
  display: flex;
  align-items: center;
  gap: 1rem;
  padding: 0.5rem 0 0.75rem;
}

.settings-profile-info {
  min-width: 0;
  flex: 1;
}

.settings-display-name {
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--text);
  line-height: 1.3;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.settings-username {
  font-size: 0.8125rem;
  color: var(--text-secondary);
  line-height: 1.3;
  margin-top: 0.125rem;
}

.settings-divider {
  height: 1px;
  background: var(--border);
  margin: 0.25rem 0;
}

.settings-group {
  padding: 0.25rem 0;
}

.settings-group .form-section-heading {
  padding: 0.25rem 0 0;
  margin: 0 0 0.25rem;
}

.menu-item--setting,
.menu-item--toggle {
  display: flex;
  align-items: center;
  gap: var(--menu-item-gap, 0.75rem);
  width: 100%;
  height: var(--menu-item-height, 48px);
  padding: 0 0.25rem;
  background: none;
  border: none;
  border-radius: 0;
  color: var(--menu-text-color, var(--text));
  font-size: 0.9375rem;
  text-align: left;
  cursor: pointer;
  transition: background var(--transition-fast);
  user-select: none;
}

.menu-item--setting:hover,
.menu-item--toggle:hover {
  background: var(--menu-hover-bg, rgba(0, 0, 0, 0.04));
  border-radius: var(--radius, 8px);
}

.menu-item__icon {
  flex-shrink: 0;
  color: var(--menu-icon-color, var(--text-secondary));
}

.menu-item__label {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.settings-select {
  width: auto;
  min-width: 120px;
  flex-shrink: 0;
}

.settings-actions {
  display: flex;
  gap: 0.5rem;
  padding: 0.75rem 0 0.25rem;
}

.settings-actions .btn {
  flex: 1;
}
</style>
