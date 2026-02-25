import { defineStore } from 'pinia'
import { ref, reactive } from 'vue'
import * as messagesApi from '@/api/messages'
import * as reactionsApi from '@/api/reactions'
import * as chatsApi from '@/api/chats'
import type { Message } from '@/api/types'

export const useMessagesStore = defineStore('messages', () => {
  const messagesByChat = ref<Record<number, Message[]>>({})
  const lastMsgId = ref<Record<number, number>>({})
  const loadingChat = ref<number | null>(null)

  // Pinned message state
  const pinnedByChat = ref<Record<number, { message: Message; pinnedAt: string; pinnedBy: number } | null>>({})
  const pinnedDismissed = ref<Set<number>>(new Set())

  // Animated deletion state
  const deletingMessages = reactive(new Set<number>())

  function markForDeletion(messageId: number): Promise<void> {
    deletingMessages.add(messageId)
    return new Promise((resolve) => setTimeout(resolve, 600))
  }

  function isDeleting(messageId: number): boolean {
    return deletingMessages.has(messageId)
  }

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
    // POST response lacks enriched reply_to fields (sender name, content, type).
    // Fill them from the local message store before inserting into the reactive array.
    enrichReplyFields(chatId, msg)
    const existingIdx = messagesByChat.value[chatId].findIndex((m) => m.id === msg.id)
    if (existingIdx !== -1) {
      messagesByChat.value[chatId][existingIdx] = msg
    } else {
      messagesByChat.value[chatId].push(msg)
    }
    lastMsgId.value[chatId] = Math.max(lastMsgId.value[chatId] || 0, msg.id)
    return msg
  }

  function enrichReplyFields(chatId: number, msg: Message) {
    if (!msg.reply_to_message_id) return
    if (msg.reply_to_sender_username || msg.reply_to_sender_name) return
    const msgs = messagesByChat.value[chatId]
    if (!msgs) return
    const original = msgs.find((m) => m.id === msg.reply_to_message_id)
    if (!original) return
    msg.reply_to_sender_username = original.sender_username
    msg.reply_to_sender_name = original.sender_display_name || original.sender_username
    if (!msg.reply_to_content && original.content) {
      msg.reply_to_content = original.content
    }
    if (!msg.reply_to_type && original.message_type) {
      msg.reply_to_type = original.message_type
    }
  }

  function pushMessage(chatId: number, msg: Message) {
    if (!messagesByChat.value[chatId]) {
      messagesByChat.value[chatId] = []
    }
    const existing = messagesByChat.value[chatId].find((m) => m.id === msg.id)
    if (!existing) {
      enrichReplyFields(chatId, msg)
      messagesByChat.value[chatId].push(msg)
      lastMsgId.value[chatId] = Math.max(lastMsgId.value[chatId] || 0, msg.id)
    }
  }

  async function deleteMessage(chatId: number, messageId: number, forEveryone = false) {
    try {
      await markForDeletion(messageId)
      await messagesApi.deleteMessage(chatId, messageId, forEveryone)
      removeMessage(chatId, messageId)
    } catch (e) {
      console.error('Failed to delete message', e)
      throw e
    } finally {
      deletingMessages.delete(messageId)
    }
  }

  function removeMessage(chatId: number, messageId: number) {
    const msgs = messagesByChat.value[chatId]
    if (!msgs) return
    messagesByChat.value[chatId] = msgs.filter((m) => m.id !== messageId)
  }

  async function editMessage(chatId: number, messageId: number, content: string) {
    try {
      const result = await messagesApi.editMessage(chatId, messageId, content)
      applyMessageUpdate(chatId, messageId, result.content, result.updated_at)
    } catch (e) {
      console.error('Failed to edit message', e)
      throw e
    }
  }

  function applyMessageUpdate(chatId: number, messageId: number, content: string, updatedAt?: string) {
    const msgs = messagesByChat.value[chatId]
    if (!msgs) return
    const msg = msgs.find((m) => m.id === messageId)
    if (!msg) return
    msg.content = content
    msg.is_edited = true
    if (updatedAt) msg.updated_at = updatedAt
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

  async function loadPinnedMessage(chatId: number) {
    try {
      const data = await messagesApi.getPinnedMessage(chatId)
      if (data && data.message) {
        pinnedByChat.value[chatId] = {
          message: data.message,
          pinnedAt: data.pinned_at,
          pinnedBy: data.pinned_by,
        }
      } else {
        pinnedByChat.value[chatId] = null
      }
    } catch (e) {
      console.error('Failed to load pinned message', e)
      pinnedByChat.value[chatId] = null
    }
  }

  async function pinMsg(chatId: number, messageId: number) {
    try {
      const data = await messagesApi.pinMessage(chatId, messageId)
      pinnedByChat.value[chatId] = {
        message: data.message,
        pinnedAt: data.pinned_at,
        pinnedBy: data.pinned_by,
      }
      pinnedDismissed.value.delete(chatId)
    } catch (e) {
      console.error('Failed to pin message', e)
      throw e
    }
  }

  async function unpinMsg(chatId: number, messageId: number) {
    try {
      await messagesApi.unpinMessage(chatId, messageId)
      pinnedByChat.value[chatId] = null
    } catch (e) {
      console.error('Failed to unpin message', e)
      throw e
    }
  }

  function applyMessagePinned(chatId: number, _messageId: number, pinnedBy: number, message: Message) {
    pinnedByChat.value[chatId] = { message, pinnedAt: new Date().toISOString(), pinnedBy }
    pinnedDismissed.value.delete(chatId)
  }

  function applyMessageUnpinned(chatId: number) {
    pinnedByChat.value[chatId] = null
  }

  function dismissPin(chatId: number) {
    pinnedDismissed.value.add(chatId)
  }

  function getMessages(chatId: number): Message[] {
    return messagesByChat.value[chatId] || []
  }

  // ── Read Receipts ──────────────────────────────────────────────────────
  // readReceipts[chatId][userId] = lastReadMsgId
  const readReceipts = ref<Record<number, Record<number, number>>>({})

  function applyReadReceipt(chatId: number, userId: number, lastReadMsgId: number) {
    if (!readReceipts.value[chatId]) {
      readReceipts.value[chatId] = {}
    }
    const current = readReceipts.value[chatId][userId] || 0
    if (lastReadMsgId > current) {
      readReceipts.value[chatId][userId] = lastReadMsgId
    }
  }

  async function loadReadReceipts(chatId: number) {
    try {
      const receipts = await chatsApi.getReadReceipts(chatId)
      if (!readReceipts.value[chatId]) {
        readReceipts.value[chatId] = {}
      }
      for (const r of receipts) {
        readReceipts.value[chatId][r.user_id] = r.last_read_msg_id
      }
    } catch (e) {
      console.error('Failed to load read receipts', e)
    }
  }

  /** DM: has the other user read this message? */
  function isMessageRead(chatId: number, messageId: number, otherUserId?: number): boolean {
    const chatReceipts = readReceipts.value[chatId]
    if (!chatReceipts) return false
    if (otherUserId) {
      return (chatReceipts[otherUserId] || 0) >= messageId
    }
    // Any user read it
    return Object.values(chatReceipts).some((lastRead) => lastRead >= messageId)
  }

  /** Group: how many members have read up to this message? */
  function getReadCount(chatId: number, messageId: number): number {
    const chatReceipts = readReceipts.value[chatId]
    if (!chatReceipts) return 0
    return Object.values(chatReceipts).filter((lastRead) => lastRead >= messageId).length
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
    editMessage,
    applyMessageUpdate,
    forwardMessages,
    pinnedByChat,
    pinnedDismissed,
    loadPinnedMessage,
    pinMsg,
    unpinMsg,
    applyMessagePinned,
    applyMessageUnpinned,
    dismissPin,
    deletingMessages,
    markForDeletion,
    isDeleting,
    readReceipts,
    applyReadReceipt,
    loadReadReceipts,
    isMessageRead,
    getReadCount,
  }
})
