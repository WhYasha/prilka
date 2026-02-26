import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import * as chatsApi from '@/api/chats'
import type { Chat } from '@/api/types'

export const useChatsStore = defineStore('chats', () => {
  const chats = ref<Chat[]>([])
  const activeChatId = ref<number | null>(null)

  // Typing indicators: chatId → { userId → expiry timer }
  const typingUsers = ref<Record<number, Record<number, { username: string; timer: ReturnType<typeof setTimeout> }>>>({})

  // Online presence: set of user IDs currently online
  const onlineUsers = ref<Set<number>>(new Set())

  // Detailed presence info: userId → { isOnline, lastSeenAt?, lastSeenBucket? }
  const userPresenceDetails = ref<Record<number, { isOnline: boolean; lastSeenAt?: string; lastSeenBucket?: string }>>({})

  const activeChat = computed(() =>
    chats.value.find((c) => c.id === activeChatId.value) ?? null,
  )

  const sortedChats = computed(() => {
    return [...chats.value].sort((a, b) => {
      // Pinned first
      if (a.is_pinned && !b.is_pinned) return -1
      if (!a.is_pinned && b.is_pinned) return 1
      // Favorites next
      if (a.is_favorite && !b.is_favorite) return -1
      if (!a.is_favorite && b.is_favorite) return 1
      // Then by updated_at descending
      const aTime = a.last_at || a.updated_at || ''
      const bTime = b.last_at || b.updated_at || ''
      return bTime.localeCompare(aTime)
    })
  })

  async function loadChats() {
    try {
      const loaded = await chatsApi.listChats()
      chats.value = loaded
      // Seed online presence from API snapshot (DM partners)
      for (const chat of loaded) {
        const raw = chat as unknown as Record<string, unknown>
        if (chat.type === 'direct' && raw.other_user_id && raw.other_is_online === true) {
          setUserOnline(raw.other_user_id as number)
        }
      }
    } catch (e) {
      console.error('Failed to load chats', e)
    }
  }

  function setActiveChat(id: number | null) {
    activeChatId.value = id
    if (id !== null) {
      const chat = chats.value.find((c) => c.id === id)
      if (chat && chat.unread_count > 0) {
        chat.unread_count = 0
      }
      // Always mark read — backend uses GREATEST so no-op if already up to date
      chatsApi.markRead(id).catch(() => {})
    }
  }

  async function toggleFavorite(chatId: number) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (!chat) return
    try {
      if (chat.is_favorite) {
        await chatsApi.unfavoriteChat(chatId)
      } else {
        await chatsApi.favoriteChat(chatId)
      }
      chat.is_favorite = !chat.is_favorite
    } catch (e) {
      console.error('Failed to toggle favorite', e)
      throw e
    }
  }

  async function toggleMute(chatId: number) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (!chat) return
    try {
      if (chat.is_muted) {
        await chatsApi.unmuteChat(chatId)
      } else {
        await chatsApi.muteChat(chatId)
      }
      chat.is_muted = !chat.is_muted
    } catch (e) {
      console.error('Failed to toggle mute', e)
      throw e
    }
  }

  async function togglePin(chatId: number) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (!chat) return
    try {
      if (chat.is_pinned) {
        await chatsApi.unpinChat(chatId)
      } else {
        await chatsApi.pinChat(chatId)
      }
      chat.is_pinned = !chat.is_pinned
    } catch (e) {
      console.error('Failed to toggle pin', e)
      throw e
    }
  }

  async function toggleArchive(chatId: number) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (!chat) return
    try {
      if (chat.is_archived) {
        await chatsApi.unarchiveChat(chatId)
      } else {
        await chatsApi.archiveChat(chatId)
      }
      chat.is_archived = !chat.is_archived
    } catch (e) {
      console.error('Failed to toggle archive', e)
      throw e
    }
  }

  async function leave(chatId: number) {
    await chatsApi.leaveChat(chatId)
    chats.value = chats.value.filter((c) => c.id !== chatId)
    if (activeChatId.value === chatId) {
      activeChatId.value = null
    }
  }

  async function deleteChat(chatId: number) {
    await chatsApi.deleteChat(chatId)
    chats.value = chats.value.filter((c) => c.id !== chatId)
    if (activeChatId.value === chatId) {
      activeChatId.value = null
    }
  }

  function updateChatLastMessage(chatId: number, content: string, time: string) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (chat) {
      chat.last_message = content
      chat.last_at = time
    }
  }

  function updateChatMetadata(
    chatId: number,
    fields: { title?: string; description?: string; avatar_url?: string },
  ) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (!chat) return
    if (fields.title !== undefined) chat.title = fields.title
    if (fields.description !== undefined) chat.description = fields.description
    if (fields.avatar_url !== undefined) (chat as Record<string, unknown>).avatar_url = fields.avatar_url
  }

  function removeChatLocal(chatId: number) {
    chats.value = chats.value.filter((c) => c.id !== chatId)
    if (activeChatId.value === chatId) {
      activeChatId.value = null
    }
  }

  function incrementUnread(chatId: number) {
    const chat = chats.value.find((c) => c.id === chatId)
    if (chat) {
      chat.unread_count = (chat.unread_count || 0) + 1
    }
  }

  function setTyping(chatId: number, userId: number, username: string) {
    if (!typingUsers.value[chatId]) {
      typingUsers.value[chatId] = {}
    }
    // Clear existing timer for this user
    const existing = typingUsers.value[chatId][userId]
    if (existing) {
      clearTimeout(existing.timer)
    }
    // Auto-expire after 3 seconds
    const timer = setTimeout(() => {
      clearTyping(chatId, userId)
    }, 3000)
    typingUsers.value[chatId][userId] = { username, timer }
  }

  function clearTyping(chatId: number, userId: number) {
    if (typingUsers.value[chatId]) {
      const entry = typingUsers.value[chatId][userId]
      if (entry) {
        clearTimeout(entry.timer)
        delete typingUsers.value[chatId][userId]
      }
    }
  }

  function getTypingUsernames(chatId: number): string[] {
    const chatTyping = typingUsers.value[chatId]
    if (!chatTyping) return []
    return Object.values(chatTyping).map((e) => e.username)
  }

  function setUserOnline(userId: number) {
    onlineUsers.value = new Set([...onlineUsers.value, userId])
    userPresenceDetails.value = {
      ...userPresenceDetails.value,
      [userId]: { isOnline: true },
    }
  }

  function setUserOffline(userId: number, lastSeenAt?: string, lastSeenBucket?: string) {
    const next = new Set(onlineUsers.value)
    next.delete(userId)
    onlineUsers.value = next
    userPresenceDetails.value = {
      ...userPresenceDetails.value,
      [userId]: { isOnline: false, lastSeenAt, lastSeenBucket },
    }
  }

  function setUserPresence(userId: number, details: { isOnline: boolean; lastSeenAt?: string; lastSeenBucket?: string }) {
    if (details.isOnline) {
      onlineUsers.value = new Set([...onlineUsers.value, userId])
    } else {
      const next = new Set(onlineUsers.value)
      next.delete(userId)
      onlineUsers.value = next
    }
    userPresenceDetails.value = {
      ...userPresenceDetails.value,
      [userId]: details,
    }
  }

  function getUserPresence(userId: number): { isOnline: boolean; lastSeenAt?: string; lastSeenBucket?: string } | undefined {
    return userPresenceDetails.value[userId]
  }

  function isUserOnline(userId: number): boolean {
    return onlineUsers.value.has(userId)
  }

  return {
    chats,
    activeChatId,
    activeChat,
    sortedChats,
    typingUsers,
    loadChats,
    setActiveChat,
    toggleFavorite,
    toggleMute,
    togglePin,
    toggleArchive,
    leave,
    deleteChat,
    updateChatLastMessage,
    updateChatMetadata,
    removeChatLocal,
    incrementUnread,
    setTyping,
    clearTyping,
    getTypingUsernames,
    onlineUsers,
    userPresenceDetails,
    setUserOnline,
    setUserOffline,
    setUserPresence,
    getUserPresence,
    isUserOnline,
  }
})
