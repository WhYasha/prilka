import { defineStore } from 'pinia'
import { ref } from 'vue'
import * as messagesApi from '@/api/messages'
import type { Message } from '@/api/types'

export const useMessagesStore = defineStore('messages', () => {
  const messagesByChat = ref<Record<number, Message[]>>({})
  const lastMsgId = ref<Record<number, number>>({})
  const loadingChat = ref<number | null>(null)

  async function loadMessages(chatId: number) {
    loadingChat.value = chatId
    try {
      const msgs = await messagesApi.listMessages(chatId, { limit: 50 })
      messagesByChat.value[chatId] = msgs
      if (msgs.length > 0) {
        lastMsgId.value[chatId] = Math.max(...msgs.map((m) => m.id))
      }
    } catch (e) {
      console.error('Failed to load messages', e)
      throw e
    } finally {
      loadingChat.value = null
    }
  }

  async function loadNewer(chatId: number) {
    const afterId = lastMsgId.value[chatId] || 0
    if (afterId === 0) return []
    try {
      const msgs = await messagesApi.listMessages(chatId, {
        after_id: afterId,
        limit: 50,
      })
      if (msgs.length > 0) {
        if (!messagesByChat.value[chatId]) {
          messagesByChat.value[chatId] = []
        }
        // Filter out duplicates
        const existingIds = new Set(messagesByChat.value[chatId].map((m) => m.id))
        const newMsgs = msgs.filter((m) => !existingIds.has(m.id))
        messagesByChat.value[chatId].push(...newMsgs)
        lastMsgId.value[chatId] = Math.max(...msgs.map((m) => m.id))
      }
      return msgs
    } catch (e) {
      console.error('Failed to load newer messages', e)
      return []
    }
  }

  async function loadOlder(chatId: number) {
    const msgs = messagesByChat.value[chatId]
    if (!msgs || msgs.length === 0) return []
    const oldest = msgs[0]
    if (!oldest) return []
    try {
      const olderMsgs = await messagesApi.listMessages(chatId, {
        before: oldest.created_at,
        limit: 50,
      })
      if (olderMsgs.length > 0) {
        const existingIds = new Set(msgs.map((m) => m.id))
        const newMsgs = olderMsgs.filter((m) => !existingIds.has(m.id))
        messagesByChat.value[chatId] = [...newMsgs, ...msgs]
      }
      return olderMsgs
    } catch (e) {
      console.error('Failed to load older messages', e)
      return []
    }
  }

  async function sendMessage(
    chatId: number,
    content: string,
    type: string = 'text',
    extra?: { sticker_id?: number; file_id?: number; duration_seconds?: number },
  ) {
    const msg = await messagesApi.sendMessage(chatId, {
      content,
      type,
      ...extra,
    })
    if (!messagesByChat.value[chatId]) {
      messagesByChat.value[chatId] = []
    }
    messagesByChat.value[chatId].push(msg)
    lastMsgId.value[chatId] = Math.max(lastMsgId.value[chatId] || 0, msg.id)
    return msg
  }

  function pushMessage(chatId: number, msg: Message) {
    if (!messagesByChat.value[chatId]) {
      messagesByChat.value[chatId] = []
    }
    const existing = messagesByChat.value[chatId].find((m) => m.id === msg.id)
    if (!existing) {
      messagesByChat.value[chatId].push(msg)
      lastMsgId.value[chatId] = Math.max(lastMsgId.value[chatId] || 0, msg.id)
    }
  }

  function getMessages(chatId: number): Message[] {
    return messagesByChat.value[chatId] || []
  }

  return {
    messagesByChat,
    lastMsgId,
    loadingChat,
    loadMessages,
    loadNewer,
    loadOlder,
    sendMessage,
    pushMessage,
    getMessages,
  }
})
