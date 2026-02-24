<template>
  <div class="drawer" :class="{ open }">
    <div class="drawer-header">
      <button class="icon-btn" aria-label="Close menu" @click="emit('close')">
        <X :size="20" :stroke-width="2" />
      </button>
      <span class="drawer-title">Menu</span>
    </div>

    <div class="drawer-avatar-wrap">
      <Avatar
        :name="authStore.user?.display_name || authStore.user?.username || '?'"
        :url="authStore.user?.avatar_url"
        size="lg"
      />
      <div class="drawer-user-info">
        <div class="drawer-display-name">
          {{ authStore.user?.display_name || authStore.user?.username || '---' }}
        </div>
        <div class="drawer-username">@{{ authStore.user?.username || '---' }}</div>
      </div>
    </div>

    <nav class="drawer-nav">
      <MenuItem :icon="UserIcon" label="My Profile" @click="emit('openProfile')" />
      <MenuItem :icon="UsersIcon" label="Create Group" @click="emit('openNewGroup')" />
      <MenuItem :icon="MegaphoneIcon" label="Create Channel" @click="emit('openNewChannel')" />
      <MenuItem :icon="StarIcon" label="Favorites" @click="emit('openFavorites')" />
      <MenuItem :icon="SettingsIcon" label="Settings" @click="emit('openSettings')" />

      <div class="drawer-divider" />

      <div class="menu-item menu-item--toggle" @click="toggleNightMode">
        <MoonIcon class="menu-item__icon" :size="24" :stroke-width="1.8" />
        <span class="menu-item__label">Night Mode</span>
        <span
          class="toggle-switch"
          :class="{ 'toggle-switch--on': settingsStore.theme === 'dark' }"
          role="switch"
          :aria-checked="settingsStore.theme === 'dark'"
        >
          <span class="toggle-switch__thumb" />
        </span>
      </div>

      <MenuItem :icon="DownloadIcon" label="Download App" @click="emit('openDownload')" />
    </nav>

    <div class="drawer-bottom">
      <div class="drawer-divider" />
      <MenuItem :icon="LogOutIcon" label="Log out" danger @click="handleLogout" />
    </div>

    <div class="drawer-footer">Simple Messenger v2</div>
  </div>
</template>

<script setup lang="ts">
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import Avatar from '@/components/ui/Avatar.vue'
import MenuItem from '@/components/layout/MenuItem.vue'
import {
  X,
  User as UserIcon,
  Users as UsersIcon,
  Megaphone as MegaphoneIcon,
  Star as StarIcon,
  Settings as SettingsIcon,
  Moon as MoonIcon,
  Download as DownloadIcon,
  LogOut as LogOutIcon,
} from 'lucide-vue-next'

defineProps<{ open: boolean }>()

const emit = defineEmits<{
  close: []
  openProfile: []
  openSettings: []
  openNewGroup: []
  openNewChannel: []
  openFavorites: []
  openDownload: []
}>()

const router = useRouter()
const authStore = useAuthStore()
const settingsStore = useSettingsStore()

function toggleNightMode() {
  const newTheme = settingsStore.theme === 'dark' ? 'light' : 'dark'
  settingsStore.applyTheme(newTheme)
  settingsStore.saveSettings({ theme: newTheme })
}

function handleLogout() {
  authStore.logout()
  router.push('/login')
}
</script>

<style scoped>
.drawer-nav {
  flex: 1;
  padding: .25rem 0;
  overflow-y: auto;
}

.drawer-divider {
  height: 1px;
  margin: .25rem 0;
  background: rgba(255, 255, 255, .07);
}

.drawer-bottom {
  flex-shrink: 0;
}

/* Night mode toggle row */
.menu-item--toggle {
  display: flex;
  align-items: center;
  gap: var(--menu-item-gap);
  width: 100%;
  height: var(--menu-item-height);
  padding: 0 var(--menu-item-px);
  background: none;
  border: none;
  color: var(--menu-text-color);
  font-size: .9375rem;
  text-align: left;
  cursor: pointer;
  transition: background var(--transition-fast);
  user-select: none;
}

.menu-item--toggle:hover {
  background: var(--menu-hover-bg);
}

.menu-item--toggle .menu-item__icon {
  flex-shrink: 0;
  width: var(--menu-item-icon-size);
  height: var(--menu-item-icon-size);
  color: var(--menu-icon-color);
}

.menu-item--toggle .menu-item__label {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* Toggle switch */
.toggle-switch {
  position: relative;
  width: 36px;
  height: 20px;
  border-radius: 10px;
  background: var(--toggle-off);
  transition: background var(--transition-fast);
  flex-shrink: 0;
}

.toggle-switch--on {
  background: var(--toggle-on);
}

.toggle-switch__thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 16px;
  height: 16px;
  border-radius: 50%;
  background: #fff;
  transition: transform var(--transition-fast);
}

.toggle-switch--on .toggle-switch__thumb {
  transform: translateX(16px);
}
</style>
