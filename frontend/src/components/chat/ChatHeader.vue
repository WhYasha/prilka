<template>
  <header class="chat-header">
    <button class="icon-btn back-btn" aria-label="Back" @click="emit('back')">&#8592;</button>
    <Avatar
      class="chat-header-avatar"
      :name="displayName"
      :url="chat?.other_avatar_url"
      :online="isOnline"
      @click="handleProfileClick"
    />
    <div class="chat-header-info" @click="handleProfileClick">
      <div class="chat-header-name">{{ displayName }}</div>
      <div class="chat-header-sub" :class="{ 'typing-text': isTyping, 'online-text': isOnline && !isTyping }">{{ subtitle }}</div>
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
  openChannelInfo: []
}>()

const chatsStore = useChatsStore()

const chat = computed(() => chatsStore.activeChat)

const displayName = computed(() => {
  const c = chat.value
  if (!c) return '---'
  if (c.type === 'channel' || c.type === 'group') return c.title || c.name || 'Untitled'
  return c.other_display_name || c.other_username || c.title || c.name || 'Chat'
})

const typingNames = computed(() => {
  if (!chat.value) return []
  return chatsStore.getTypingUsernames(chat.value.id)
})

const isTyping = computed(() => typingNames.value.length > 0)

const isOnline = computed(() => {
  const c = chat.value
  if (!c || c.type !== 'direct' || !c.other_user_id) return false
  return chatsStore.isUserOnline(c.other_user_id)
})

const subtitle = computed(() => {
  // Show typing indicator if someone is typing
  if (typingNames.value.length === 1) {
    return `${typingNames.value[0]} is typing...`
  }
  if (typingNames.value.length > 1) {
    return `${typingNames.value.length} people typing...`
  }

  const c = chat.value
  if (!c) return ''
  if (c.type === 'channel') {
    const count = c.member_count ?? 0
    return `${count} subscriber${count !== 1 ? 's' : ''}`
  }
  if (c.type === 'group') {
    const count = c.member_count ?? 0
    return `${count} member${count !== 1 ? 's' : ''}`
  }
  // For DMs: show "online" or @username
  if (isOnline.value) return 'online'
  return c.other_username ? '@' + c.other_username : ''
})

function handleProfileClick() {
  const c = chat.value
  if (!c) return
  if (c.type === 'channel' || c.type === 'group') {
    emit('openChannelInfo')
  } else if (c.type === 'direct' && c.other_username) {
    emit('openUserProfile', c.other_username)
  }
}
</script>
