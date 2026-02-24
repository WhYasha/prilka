<template>
  <div class="drawer" :class="{ open }">
    <div class="drawer-header">
      <button class="icon-btn" aria-label="Close menu" @click="emit('close')">&#10005;</button>
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
      <button class="drawer-nav-item" @click="emit('openProfile')">
        Profile &amp; Settings
      </button>
      <button class="drawer-nav-item" @click="emit('openDownload')">
        Download App
      </button>
      <button class="drawer-nav-item" @click="handleLogout">
        Log out
      </button>
    </nav>
    <div class="drawer-footer">Simple Messenger v2</div>
  </div>
</template>

<script setup lang="ts">
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import Avatar from '@/components/ui/Avatar.vue'

defineProps<{ open: boolean }>()

const emit = defineEmits<{
  close: []
  openProfile: []
  openDownload: []
}>()

const router = useRouter()
const authStore = useAuthStore()

function handleLogout() {
  authStore.logout()
  router.push('/login')
}
</script>
