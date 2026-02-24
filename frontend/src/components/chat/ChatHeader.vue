<template>
  <header class="chat-header">
    <button class="icon-btn back-btn" aria-label="Back" @click="emit('back')">&#8592;</button>
    <Avatar
      class="chat-header-avatar"
      :name="displayName"
      :url="chat?.other_avatar_url"
      @click="handleProfileClick"
    />
    <div class="chat-header-info" @click="handleProfileClick">
      <div class="chat-header-name">{{ displayName }}</div>
      <div class="chat-header-sub">{{ subtitle }}</div>
    </div>
  </header>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useChatsStore } from '@/stores/chats'
import Avatar from '@/components/ui/Avatar.vue'

const emit = defineEmits<{
  back: []
  openUserProfile: [username: string]
}>()

const chatsStore = useChatsStore()

const chat = computed(() => chatsStore.activeChat)

const displayName = computed(() => {
  const c = chat.value
  if (!c) return '---'
  if (c.type === 'channel' || c.type === 'group') return c.title || c.name || 'Untitled'
  return c.other_display_name || c.other_username || c.title || c.name || 'Chat'
})

const subtitle = computed(() => {
  const c = chat.value
  if (!c) return ''
  if (c.type === 'channel') return 'Channel'
  if (c.type === 'group') return 'Group'
  return c.other_username ? '@' + c.other_username : ''
})

function handleProfileClick() {
  if (chat.value?.type === 'direct' && chat.value.other_username) {
    emit('openUserProfile', chat.value.other_username)
  }
}
</script>
