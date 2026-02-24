import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import * as chatsApi from '@/api/chats'
import type { Chat } from '@/api/types'

export const useChatsStore = defineStore('chats', () => {
  const chats = ref<Chat[]>([])
  const activeChatId = ref<number | null>(null)

  const activeChat = computed(() =>
    chats.value.find((c) => c.id === activeChatId.value) ?? null,
  )

  const sortedChats = computed(() => {
    return [...chats.value].sort((a, b) => {
      // Favorites first
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
      chats.value = await chatsApi.listChats()
    } catch (e) {
      console.error('Failed to load chats', e)
    }
  }

  function setActiveChat(id: number | null) {
    activeChatId.value = id
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

  async function leave(chatId: number) {
    await chatsApi.leaveChat(chatId)
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

  return {
    chats,
    activeChatId,
    activeChat,
    sortedChats,
    loadChats,
    setActiveChat,
    toggleFavorite,
    toggleMute,
    leave,
    updateChatLastMessage,
  }
})
