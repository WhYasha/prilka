<template>
  <header class="chat-header">
    <button class="icon-btn back-btn" aria-label="Back" @click="emit('back')">&#8592;</button>
    <Avatar
      class="chat-header-avatar"
      :name="displayName"
      :url="chatAvatarUrl"
      :online="isOnline"
      size="sm"
      @click="handleProfileClick"
    />
    <div class="chat-header-info" @click="handleProfileClick">
      <div class="chat-header-name">{{ displayName }}</div>
      <div v-if="subtitle" class="chat-header-sub" :class="{ 'typing-text': isTyping, 'online-text': isOnline && !isTyping }">{{ subtitle }}<span v-if="isTyping" class="typing-dots"><span class="typing-dot" /><span class="typing-dot" /><span class="typing-dot" /></span></div>
    </div>
    <button class="icon-btn search-btn" aria-label="Search messages" @click="emit('openSearch')">&#128269;</button>
  </header>
</template>

<script setup lang="ts">
import { computed, watch } from 'vue'
import { useChatsStore } from '@/stores/chats'
import { useAuthStore } from '@/stores/auth'
import { formatLastSeen } from '@/utils/formatLastSeen'
import { getUser } from '@/api/users'
import Avatar from '@/components/ui/Avatar.vue'

const emit = defineEmits<{
  back: []
  openUserProfile: [username: string]
  openChannelInfo: []
  openSearch: []
}>()

const chatsStore = useChatsStore()
const authStore = useAuthStore()

const chat = computed(() => chatsStore.activeChat)

const isSelfChat = computed(() => {
  const c = chat.value
  return c?.type === 'direct' && !c.other_user_id
})

const displayName = computed(() => {
  const c = chat.value
  if (!c) return '---'
  if (isSelfChat.value) return 'Saved Messages'
  if (c.type === 'channel' || c.type === 'group') return c.title || c.name || 'Untitled'
  return c.other_display_name || c.other_username || c.title || c.name || 'Chat'
})

const chatAvatarUrl = computed(() => {
  const c = chat.value
  if (!c) return undefined
  if (isSelfChat.value) return authStore.user?.avatar_url
  if (c.type === 'direct') return c.other_avatar_url
  return c.avatar_url
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
  if (typingNames.value.length === 1) {
    return `${typingNames.value[0]} is typing`
  }
  if (typingNames.value.length > 1) {
    return `${typingNames.value.length} people typing`
  }

  const c = chat.value
  if (!c) return ''
  if (isSelfChat.value) return 'personal storage'
  if (c.type === 'channel') {
    const count = c.member_count ?? 0
    return `${count} subscriber${count !== 1 ? 's' : ''}`
  }
  if (c.type === 'group') {
    const count = c.member_count ?? 0
    return `${count} member${count !== 1 ? 's' : ''}`
  }
  if (isOnline.value) return 'online'

  // Show last-seen info for DM chats
  if (c.type === 'direct' && c.other_user_id) {
    const presence = chatsStore.getUserPresence(c.other_user_id)
    if (presence) {
      if (presence.lastSeenAt) {
        return formatLastSeen(presence.lastSeenAt)
      }
      if (presence.lastSeenBucket) {
        return `last seen ${presence.lastSeenBucket}`
      }
    }
  }

  return c.other_username ? '@' + c.other_username : ''
})

// Fetch initial presence data when opening a DM chat
watch(
  () => chat.value?.id,
  () => {
    const c = chat.value
    if (!c || c.type !== 'direct' || !c.other_user_id) return
    const userId = c.other_user_id
    // Only fetch if we don't have presence details yet
    if (!chatsStore.getUserPresence(userId)) {
      getUser(userId)
        .then((user) => {
          if (user.last_activity) {
            chatsStore.setUserPresence(userId, {
              isOnline: chatsStore.isUserOnline(userId),
              lastSeenAt: user.last_activity,
            })
          }
        })
        .catch(() => {})
    }
  },
  { immediate: true },
)

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

<style scoped>
.typing-dots {
  display: inline-flex;
  gap: 2px;
  margin-left: 2px;
}

.typing-dot {
  width: 4px;
  height: 4px;
  border-radius: 50%;
  background: var(--accent);
  animation: typing-dot-bounce .6s infinite;
}

.typing-dot:nth-child(2) {
  animation-delay: .15s;
}

.typing-dot:nth-child(3) {
  animation-delay: .3s;
}
</style>
