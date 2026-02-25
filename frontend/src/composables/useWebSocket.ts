import { ref, onUnmounted } from 'vue'
import { useMessagesStore } from '@/stores/messages'
import { useChatsStore } from '@/stores/chats'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import type { Message } from '@/api/types'

function showMessageNotification(
  msg: Message,
  chatsStore: ReturnType<typeof useChatsStore>,
  authStore: ReturnType<typeof useAuthStore>,
) {
  if (!document.hidden) return
  if (msg.sender_id === authStore.user?.id) return
  const settingsStore = useSettingsStore()
  if (!settingsStore.notificationsEnabled) return
  if (!('Notification' in window) || Notification.permission !== 'granted') return
  const chat = chatsStore.chats.find((c) => c.id === msg.chat_id)
  if (chat?.is_muted) return

  const senderName = msg.sender_display_name || msg.sender_username || 'Someone'
  const body =
    msg.message_type === 'text'
      ? msg.content?.length > 100
        ? msg.content.substring(0, 100) + '...'
        : msg.content
      : `sent a ${msg.message_type === 'sticker' ? 'sticker' : msg.message_type === 'voice' ? 'voice message' : 'file'}`

  const n = new Notification(senderName, {
    body,
    tag: `chat-${msg.chat_id}`,
    silent: false,
  })
  n.onclick = () => {
    window.focus()
    chatsStore.setActiveChat(msg.chat_id)
    n.close()
  }
  setTimeout(() => n.close(), 5000)
}

export function useWebSocket() {
  const connected = ref(false)
  let ws: WebSocket | null = null
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null
  let pingTimer: ReturnType<typeof setInterval> | null = null
  let reconnectDelay = 1000
  let intentionalClose = false

  function getWsUrl() {
    const token = localStorage.getItem('access_token')
    if (!token) return null
    const wsBase = import.meta.env.VITE_WS_URL
    if (wsBase) {
      return `${wsBase}?token=${encodeURIComponent(token)}`
    }
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    return `${proto}//${window.location.host}/ws?token=${encodeURIComponent(token)}`
  }

  function connect() {
    if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) {
      return
    }

    const url = getWsUrl()
    if (!url) return

    intentionalClose = false
    ws = new WebSocket(url)

    ws.onopen = () => {
      connected.value = true
      reconnectDelay = 1000

      // Send auth
      const token = localStorage.getItem('access_token')
      if (token) {
        ws!.send(JSON.stringify({ type: 'auth', token }))
      }

      // Subscribe to all chats
      const chatsStore = useChatsStore()
      for (const chat of chatsStore.chats) {
        ws!.send(JSON.stringify({ type: 'subscribe', chat_id: chat.id }))
      }

      // Start heartbeat
      startHeartbeat()
    }

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data)
        handleMessage(data)
      } catch {
        // ignore non-JSON messages
      }
    }

    ws.onclose = () => {
      connected.value = false
      stopHeartbeat()
      if (!intentionalClose) {
        scheduleReconnect()
      }
    }

    ws.onerror = () => {
      // onclose will fire after onerror
    }
  }

  function handleMessage(data: Record<string, unknown>) {
    const messagesStore = useMessagesStore()
    const chatsStore = useChatsStore()
    const authStore = useAuthStore()

    switch (data.type) {
      case 'message': {
        const msg = data as unknown as Message
        messagesStore.pushMessage(msg.chat_id, msg)
        chatsStore.updateChatLastMessage(
          msg.chat_id,
          msg.content || (msg.message_type === 'sticker' ? 'Sticker' : 'Attachment'),
          msg.created_at,
        )
        // Increment unread if this chat is not currently active
        // and the message is from someone else
        if (msg.chat_id !== chatsStore.activeChatId && msg.sender_id !== authStore.user?.id) {
          chatsStore.incrementUnread(msg.chat_id)
        }
        // Clear typing indicator for the sender (they sent a message)
        chatsStore.clearTyping(msg.chat_id, msg.sender_id)
        // Browser notification for background tab
        showMessageNotification(msg, chatsStore, authStore)
        break
      }
      case 'typing': {
        const chatId = data.chat_id as number
        const userId = data.user_id as number
        // Don't show typing for ourselves
        if (userId !== authStore.user?.id) {
          // Look up username from chat members or use a placeholder
          const username = (data.username as string) || 'Someone'
          chatsStore.setTyping(chatId, userId, username)
        }
        break
      }
      case 'pong':
        // heartbeat acknowledged
        break
      case 'presence': {
        const userId = data.user_id as number
        const status = data.status as string
        const lastSeenAt = data.last_seen_at as string | undefined
        const lastSeenBucket = data.last_seen_bucket as string | undefined
        if (status === 'online') {
          chatsStore.setUserOnline(userId)
        } else if (status === 'offline') {
          chatsStore.setUserOffline(userId, lastSeenAt, lastSeenBucket)
        }
        break
      }
      case 'message_deleted': {
        const chatId = data.chat_id as number
        const messageId = data.message_id as number
        messagesStore.removeMessage(chatId, messageId)
        break
      }
      case 'message_updated': {
        const chatId = data.chat_id as number
        const messageId = data.message_id as number
        const content = data.content as string
        const updatedAt = data.updated_at as string
        messagesStore.applyMessageUpdate(chatId, messageId, content, updatedAt)
        break
      }
      case 'message_pinned': {
        const chatId = data.chat_id as number
        const messageId = data.message_id as number
        const pinnedBy = data.pinned_by as number
        const message = data.message as Message
        messagesStore.applyMessagePinned(chatId, messageId, pinnedBy, message)
        break
      }
      case 'message_unpinned': {
        const chatId = data.chat_id as number
        messagesStore.applyMessageUnpinned(chatId)
        break
      }
      case 'reaction': {
        const chatId = data.chat_id as number
        const messageId = data.message_id as number
        const userId = data.user_id as number
        const emoji = data.emoji as string
        const action = data.action as 'added' | 'removed'
        messagesStore.applyReactionUpdate(chatId, messageId, action, emoji, userId === authStore.user?.id)
        break
      }
    }
  }

  function startHeartbeat() {
    stopHeartbeat()
    pingTimer = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'ping' }))
      }
    }, 25000)
  }

  function stopHeartbeat() {
    if (pingTimer) {
      clearInterval(pingTimer)
      pingTimer = null
    }
  }

  function scheduleReconnect() {
    if (reconnectTimer) return
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null
      connect()
      reconnectDelay = Math.min(reconnectDelay * 2, 30000)
    }, reconnectDelay)
  }

  function subscribeToChat(chatId: number) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'subscribe', chat_id: chatId }))
    }
  }

  function sendTyping(chatId: number) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'typing', chat_id: chatId }))
    }
  }

  function disconnect() {
    intentionalClose = true
    stopHeartbeat()
    if (reconnectTimer) {
      clearTimeout(reconnectTimer)
      reconnectTimer = null
    }
    if (ws) {
      ws.close()
      ws = null
    }
    connected.value = false
  }

  onUnmounted(() => {
    disconnect()
  })

  return {
    connected,
    connect,
    disconnect,
    subscribeToChat,
    sendTyping,
  }
}
