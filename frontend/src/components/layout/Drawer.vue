<template>
  <div class="drawer" :class="{ open }">
    <div class="drawer-profile">
      <Avatar
        :name="authStore.user?.display_name || authStore.user?.username || '?'"
        :url="authStore.user?.avatar_url"
        size="xl"
      />
      <div class="drawer-profile-bottom">
        <div class="drawer-profile-info">
          <div class="drawer-display-name">
            {{ authStore.user?.display_name || authStore.user?.username || '---' }}
          </div>
          <div class="drawer-username">@{{ authStore.user?.username || '---' }}</div>
        </div>
        <button class="icon-btn drawer-edit-btn" aria-label="Edit profile" @click="emit('openProfile')">
          <PencilIcon :size="18" :stroke-width="2" />
        </button>
      </div>
    </div>

    <nav class="drawer-nav">
      <template v-for="item in visibleMenuItems" :key="item.id">
        <div v-if="item.separator" class="drawer-divider" />

        <!-- Night mode toggle has special rendering -->
        <div v-if="item.rightSlot === 'toggle'" class="menu-item menu-item--toggle" @click="handleAction(item.action)">
          <component :is="iconMap[item.icon]" class="menu-item__icon" :size="24" :stroke-width="1.8" />
          <span class="menu-item__label">{{ item.label }}</span>
          <span
            class="toggle-switch"
            :class="{ 'toggle-switch--on': settingsStore.theme === 'dark' }"
            role="switch"
            :aria-checked="settingsStore.theme === 'dark'"
          >
            <span class="toggle-switch__thumb" />
          </span>
        </div>

        <!-- Logout gets danger styling -->
        <MenuItem
          v-else-if="item.id === 'logout'"
          :icon="iconMap[item.icon]"
          :label="item.label"
          danger
          @click="handleAction(item.action)"
        />

        <!-- Regular menu items -->
        <MenuItem
          v-else
          :icon="iconMap[item.icon]"
          :label="item.label"
          @click="handleAction(item.action)"
        />
      </template>
    </nav>

    <div class="drawer-footer">BeHappy v2</div>
  </div>
</template>

<script setup lang="ts">
import { computed, type Component } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { getVisibleMenuItems, type MenuContext } from '@/config/menuConfig'
import Avatar from '@/components/ui/Avatar.vue'
import MenuItem from '@/components/layout/MenuItem.vue'
import {
  Pencil as PencilIcon,
  User as UserIcon,
  Users as UsersIcon,
  Megaphone as MegaphoneIcon,
  Star as StarIcon,
  Settings as SettingsIcon,
  Moon as MoonIcon,
  Download as DownloadIcon,
  LogOut as LogOutIcon,
  Bookmark as BookmarkIcon,
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

// Icon mapping from menuConfig string names to Lucide components
const iconMap: Record<string, Component> = {
  user: UserIcon,
  users: UsersIcon,
  megaphone: MegaphoneIcon,
  bookmark: BookmarkIcon,
  settings: SettingsIcon,
  moon: MoonIcon,
  download: DownloadIcon,
  'log-out': LogOutIcon,
  star: StarIcon,
}

const isTauri = '__TAURI_INTERNALS__' in window

const menuContext = computed<MenuContext>(() => ({
  isAuthenticated: !!authStore.user,
  isDesktop: true,
  isTauri,
}))

const visibleMenuItems = computed(() => getVisibleMenuItems(menuContext.value))

// Action handlers mapped by action name
const actionHandlers: Record<string, () => void> = {
  openProfile: () => emit('openProfile'),
  createGroup: () => emit('openNewGroup'),
  createChannel: () => emit('openNewChannel'),
  openFavorites: () => emit('openFavorites'),
  openSettings: () => emit('openSettings'),
  toggleNightMode: () => {
    const newTheme = settingsStore.theme === 'dark' ? 'light' : 'dark'
    settingsStore.applyTheme(newTheme)
    settingsStore.saveSettings({ theme: newTheme })
  },
  openDownload: () => emit('openDownload'),
  logout: () => {
    authStore.logout()
    router.push('/login')
  },
}

function handleAction(action?: string) {
  if (action && actionHandlers[action]) {
    actionHandlers[action]()
  }
}
</script>

<style scoped>
.drawer-profile {
  padding: 1rem 1rem .75rem;
  display: flex;
  flex-direction: column;
  gap: .75rem;
}

.drawer-profile-bottom {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
}

.drawer-profile-info {
  min-width: 0;
  flex: 1;
}

.drawer-display-name {
  font-size: 1rem;
  font-weight: 600;
  color: var(--menu-text-color);
  line-height: 1.3;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.drawer-username {
  font-size: .8125rem;
  color: var(--text-secondary);
  line-height: 1.3;
  margin-top: .125rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.drawer-edit-btn {
  flex-shrink: 0;
  color: var(--text-secondary);
  opacity: .7;
  transition: opacity var(--transition-fast);
}

.drawer-edit-btn:hover {
  opacity: 1;
}

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
