import { ref, onUnmounted } from 'vue'
import { useMessagesStore } from '@/stores/messages'
import { useChatsStore } from '@/stores/chats'
import type { Message } from '@/api/types'

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

    switch (data.type) {
      case 'message': {
        const msg = data as unknown as Message
        messagesStore.pushMessage(msg.chat_id, msg)
        chatsStore.updateChatLastMessage(
          msg.chat_id,
          msg.content || (msg.message_type === 'sticker' ? 'Sticker' : 'Attachment'),
          msg.created_at,
        )
        break
      }
      case 'pong':
        // heartbeat acknowledged
        break
      case 'presence':
        // Could update online status here
        break
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
  }
}
