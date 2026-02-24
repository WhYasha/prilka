import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import * as chatsApi from '@/api/chats'
import type { Chat, ChatMember } from '@/api/types'
import { useAuthStore } from '@/stores/auth'

export const useChannelStore = defineStore('channel', () => {
  const detail = ref<Chat | null>(null)
  const members = ref<ChatMember[]>([])
  const loading = ref(false)

  const memberCount = computed(() => members.value.length)

  const adminCount = computed(
    () => members.value.filter((m) => m.role === 'admin' || m.role === 'owner').length,
  )

  const myRole = computed(() => {
    const auth = useAuthStore()
    if (!auth.user) return null
    const me = members.value.find((m) => m.id === auth.user!.id)
    return me?.role ?? null
  })

  const isOwner = computed(() => myRole.value === 'owner')

  const isAdmin = computed(() => myRole.value === 'admin' || myRole.value === 'owner')

  async function loadDetail(chatId: number) {
    loading.value = true
    try {
      detail.value = await chatsApi.getChat(chatId)
    } catch (e) {
      console.error('Failed to load channel detail', e)
    } finally {
      loading.value = false
    }
  }

  async function loadMembers(chatId: number) {
    try {
      members.value = await chatsApi.getChatMembers(chatId)
    } catch (e) {
      console.error('Failed to load channel members', e)
    }
  }

  async function updateChannel(
    chatId: number,
    payload: { title?: string; description?: string; public_name?: string },
  ) {
    try {
      detail.value = await chatsApi.updateChat(chatId, payload)
    } catch (e) {
      console.error('Failed to update channel', e)
      throw e
    }
  }

  return {
    detail,
    members,
    loading,
    memberCount,
    adminCount,
    myRole,
    isOwner,
    isAdmin,
    loadDetail,
    loadMembers,
    updateChannel,
  }
})
