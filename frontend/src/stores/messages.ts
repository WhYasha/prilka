import { defineStore } from 'pinia'
import { ref } from 'vue'
import * as messagesApi from '@/api/messages'
import * as reactionsApi from '@/api/reactions'
import type { Message } from '@/api/types'

export const useMessagesStore = defineStore('messages', () => {
  const messagesByChat = ref<Record<number, Message[]>>({})
  const lastMsgId = ref<Record<number, number>>({})
  const loadingChat = ref<number | null>(null)

  async function loadReactions(chatId: number) {
    const msgs = messagesByChat.value[chatId]
    if (!msgs || msgs.length === 0) return
    const ids = msgs.map((m) => m.id)
    try {
      const map = await reactionsApi.getReactions(chatId, ids)
      for (const m of msgs) {
        m.reactions = map[String(m.id)] || undefined
      }
    } catch (e) {
      console.error('Failed to load reactions', e)
    }
  }

  async function loadMessages(chatId: number) {
    loadingChat.value = chatId
    try {
      const msgs = await messagesApi.listMessages(chatId, { limit: 50 })
      messagesByChat.value[chatId] = msgs
      if (msgs.length > 0) {
        lastMsgId.value[chatId] = Math.max(...msgs.map((m) => m.id))
      }
      await loadReactions(chatId)
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
        await loadReactions(chatId)
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
        await loadReactions(chatId)
      }
      return olderMsgs
    } catch (e) {
      console.error('Failed to load older messages', e)
      return []
    }
  }

  async function toggleReaction(chatId: number, messageId: number, emoji: string) {
    try {
      const result = await reactionsApi.toggleReaction(chatId, messageId, emoji)
      applyReactionUpdate(chatId, messageId, result.action, emoji, true)
    } catch (e) {
      console.error('Failed to toggle reaction', e)
    }
  }

  function applyReactionUpdate(
    chatId: number,
    messageId: number,
    action: 'added' | 'removed',
    emoji: string,
    isMe: boolean,
  ) {
    const msgs = messagesByChat.value[chatId]
    if (!msgs) return
    const msg = msgs.find((m) => m.id === messageId)
    if (!msg) return

    if (!msg.reactions) msg.reactions = []

    const existing = msg.reactions.find((r) => r.emoji === emoji)
    if (action === 'added') {
      if (existing) {
        existing.count++
        if (isMe) existing.me = true
      } else {
        msg.reactions.push({ emoji, count: 1, me: isMe })
      }
    } else {
      // removed
      if (existing) {
        existing.count--
        if (isMe) existing.me = false
        if (existing.count <= 0) {
          msg.reactions = msg.reactions.filter((r) => r.emoji !== emoji)
        }
      }
    }
    // Clean up empty array
    if (msg.reactions.length === 0) msg.reactions = undefined
  }

  async function sendMessage(
    chatId: number,
    content: string,
    type: string = 'text',
    extra?: { sticker_id?: number; file_id?: number; duration_seconds?: number; reply_to_message_id?: number },
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

  async function deleteMessage(chatId: number, messageId: number) {
    try {
      await messagesApi.deleteMessage(chatId, messageId)
      removeMessage(chatId, messageId)
    } catch (e) {
      console.error('Failed to delete message', e)
      throw e
    }
  }

  function removeMessage(chatId: number, messageId: number) {
    const msgs = messagesByChat.value[chatId]
    if (!msgs) return
    messagesByChat.value[chatId] = msgs.filter((m) => m.id !== messageId)
  }

  async function forwardMessages(toChatId: number, fromChatId: number, messageIds: number[]) {
    try {
      const msgs = await messagesApi.forwardMessages(toChatId, {
        from_chat_id: fromChatId,
        message_ids: messageIds,
      })
      for (const msg of msgs) {
        pushMessage(toChatId, msg)
      }
      return msgs
    } catch (e) {
      console.error('Failed to forward messages', e)
      throw e
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
    toggleReaction,
    applyReactionUpdate,
    deleteMessage,
    removeMessage,
    forwardMessages,
  }
})
