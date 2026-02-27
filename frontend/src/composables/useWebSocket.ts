import { ref, onUnmounted } from 'vue'
import { useMessagesStore } from '@/stores/messages'
import { useChatsStore } from '@/stores/chats'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { markRead } from '@/api/chats'
import type { Message } from '@/api/types'

function showMessageNotification(
  msg: Message,
  chatsStore: ReturnType<typeof useChatsStore>,
  authStore: ReturnType<typeof useAuthStore>,
) {
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
  let fallbackPollTimer: ReturnType<typeof setInterval> | null = null
  let reconnectDelay = 1000
  let intentionalClose = false
  let hasConnectedBefore = false
  let disconnectedSince: number | null = null

  // ── Unified activity-based presence ─────────────────────────────────
  // Single source of truth: real user interaction determines online/offline.
  // document.hasFocus() is NOT used — unreliable in Tauri.
  const ACTIVITY_TIMEOUT = 30_000 // 30s without interaction → away
  let lastUserActivity = Date.now()
  let lastActivityRefreshSent = 0
  let isPresenceActive = false
  let presenceCheckTimer: ReturnType<typeof setInterval> | null = null
  let presenceSetup = false
  let awayGraceTimer: ReturnType<typeof setTimeout> | null = null
  let windowHasFocus = true

  // Deduplicate presence events: backend broadcasts to each shared chat,
  // so observer in N shared chats receives N identical events per status change.
  const presenceCache = new Map<number, string>()

  // Debounced markRead for messages arriving in the active visible chat
  let markReadTimer: ReturnType<typeof setTimeout> | null = null
  let pendingMarkReadChatId: number | null = null

  function debouncedMarkRead(chatId: number) {
    pendingMarkReadChatId = chatId
    if (markReadTimer) return
    markReadTimer = setTimeout(() => {
      markReadTimer = null
      if (pendingMarkReadChatId !== null) {
        markRead(pendingMarkReadChatId).catch(() => {})
        pendingMarkReadChatId = null
      }
    }, 1000)
  }

  function sendPresenceUpdate(status: 'active' | 'away') {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'presence_update', status }))
    }
  }

  function onUserActivity() {
    if (!windowHasFocus) return // ignore mouse passing over unfocused window
    lastUserActivity = Date.now()
    // Immediate away→active transition on real interaction
    if (!isPresenceActive && !document.hidden) {
      isPresenceActive = true
      sendPresenceUpdate('active')
    }
  }

  function startAwayGrace() {
    if (isPresenceActive && !awayGraceTimer) {
      awayGraceTimer = setTimeout(() => {
        awayGraceTimer = null
        if (isPresenceActive) {
          isPresenceActive = false
          sendPresenceUpdate('away')
        }
      }, 3_000)
    }
  }

  function cancelAwayGrace() {
    if (awayGraceTimer) {
      clearTimeout(awayGraceTimer)
      awayGraceTimer = null
    }
  }

  function onVisibilityChange() {
    if (document.hidden) {
      startAwayGrace()
    } else {
      cancelAwayGrace()
      if (!isPresenceActive && Date.now() - lastUserActivity < ACTIVITY_TIMEOUT) {
        isPresenceActive = true
        sendPresenceUpdate('active')
      }
      // Mark active chat as read when tab becomes visible again (only if near bottom)
      const chatsStore = useChatsStore()
      if (chatsStore.activeChatId && chatsStore.isNearBottom) {
        const chat = chatsStore.chats.find((c) => c.id === chatsStore.activeChatId)
        if (chat && chat.unread_count > 0) chat.unread_count = 0
        markRead(chatsStore.activeChatId).catch(() => {})
      }
    }
  }

  function onWindowBlur() {
    windowHasFocus = false
    // Fires on Tauri minimize, browser Alt+Tab, etc. — document.hidden may NOT change
    startAwayGrace()
  }

  function onWindowFocus() {
    windowHasFocus = true
    cancelAwayGrace()
    lastUserActivity = Date.now()
    if (!isPresenceActive) {
      isPresenceActive = true
      sendPresenceUpdate('active')
    }
    // Mark read only if near bottom (user can see latest messages)
    const chatsStore = useChatsStore()
    if (chatsStore.activeChatId && chatsStore.isNearBottom) {
      const chat = chatsStore.chats.find((c) => c.id === chatsStore.activeChatId)
      if (chat && chat.unread_count > 0) chat.unread_count = 0
      markRead(chatsStore.activeChatId).catch(() => {})
    }
  }

  function presenceCheck() {
    if (document.hidden) return // handled by onVisibilityChange
    if (Date.now() - lastUserActivity > ACTIVITY_TIMEOUT && isPresenceActive) {
      isPresenceActive = false
      sendPresenceUpdate('away')
    }
  }

  function setupPresence() {
    if (presenceSetup) return
    presenceSetup = true
    const opts: AddEventListenerOptions = { passive: true, capture: true }
    document.addEventListener('mousemove', onUserActivity, opts)
    document.addEventListener('keydown', onUserActivity, opts)
    document.addEventListener('click', onUserActivity, opts)
    document.addEventListener('scroll', onUserActivity, opts)
    document.addEventListener('touchstart', onUserActivity, opts)
    document.addEventListener('visibilitychange', onVisibilityChange)
    window.addEventListener('blur', onWindowBlur)
    window.addEventListener('focus', onWindowFocus)
    presenceCheckTimer = setInterval(presenceCheck, 15_000)
  }

  function teardownPresence() {
    if (!presenceSetup) return
    presenceSetup = false
    const opts: EventListenerOptions = { capture: true }
    document.removeEventListener('mousemove', onUserActivity, opts)
    document.removeEventListener('keydown', onUserActivity, opts)
    document.removeEventListener('click', onUserActivity, opts)
    document.removeEventListener('scroll', onUserActivity, opts)
    document.removeEventListener('touchstart', onUserActivity, opts)
    document.removeEventListener('visibilitychange', onVisibilityChange)
    window.removeEventListener('blur', onWindowBlur)
    window.removeEventListener('focus', onWindowFocus)
    if (awayGraceTimer) {
      clearTimeout(awayGraceTimer)
      awayGraceTimer = null
    }
    if (presenceCheckTimer) {
      clearInterval(presenceCheckTimer)
      presenceCheckTimer = null
    }
  }

  function getWsUrl() {
    const token = localStorage.getItem('access_token')
    if (!token) return null
    const wsBase = import.meta.env.VITE_WS_URL
    if (wsBase) {
      return wsBase
    }
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    return `${proto}//${window.location.host}/ws`
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
      disconnectedSince = null

      // Stop fallback polling — WS is back
      stopFallbackPolling()

      // Reset presence state on (re)connect
      lastUserActivity = Date.now()
      lastActivityRefreshSent = 0
      isPresenceActive = !document.hidden

      // Send auth with current active state
      const token = localStorage.getItem('access_token')
      if (token) {
        ws!.send(JSON.stringify({ type: 'auth', token, active: isPresenceActive }))
      }

      // Subscribe to all chats
      const chatsStore = useChatsStore()
      for (const chat of chatsStore.chats) {
        ws!.send(JSON.stringify({ type: 'subscribe', chat_id: chat.id }))
      }

      // Resync on reconnect (not first connect) — one-shot fetch
      if (hasConnectedBefore) {
        const messagesStore = useMessagesStore()
        chatsStore.loadChats()
        if (chatsStore.activeChatId) {
          messagesStore.loadNewer(chatsStore.activeChatId)
        }
      }
      hasConnectedBefore = true

      // Start heartbeat and unified presence tracking
      startHeartbeat()
      setupPresence()
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
        disconnectedSince = disconnectedSince || Date.now()
        scheduleReconnect()
        // Start fallback polling only if disconnected for >10 seconds
        startFallbackPolling()
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
        if (msg.sender_id !== authStore.user?.id) {
          const willBeRead = msg.chat_id === chatsStore.activeChatId
            && isPresenceActive && windowHasFocus && chatsStore.isNearBottom
          if (willBeRead) {
            debouncedMarkRead(msg.chat_id)
          } else {
            chatsStore.incrementUnread(msg.chat_id)
          }
        }
        // Clear typing indicator for the sender (they sent a message)
        chatsStore.clearTyping(msg.chat_id, msg.sender_id)
        // Show notification when user is not actively using the app
        if (!isPresenceActive || !windowHasFocus || document.hidden) {
          showMessageNotification(msg, chatsStore, authStore)
        }
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
        // Skip if this user's status hasn't actually changed
        if (presenceCache.get(userId) === status) break
        presenceCache.set(userId, status)

        const lastSeenAt = data.last_seen_at as string | undefined
        const lastSeenBucket = data.last_seen_bucket as string | undefined
        if (status === 'online') {
          chatsStore.setUserOnline(userId)
        } else if (status === 'offline') {
          const effectiveLastSeenAt = lastSeenAt || new Date().toISOString()
          chatsStore.setUserOffline(userId, effectiveLastSeenAt, lastSeenBucket)
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
      case 'read_receipt': {
        const chatId = data.chat_id as number
        const userId = data.user_id as number
        const lastReadMsgId = data.last_read_msg_id as number
        messagesStore.applyReadReceipt(chatId, userId, lastReadMsgId)
        break
      }
      case 'chat_created': {
        // New chat was created — reload chat list and subscribe
        const chatId = data.chat_id as number
        chatsStore.loadChats().then(() => {
          subscribeToChat(chatId)
        })
        break
      }
      case 'chat_updated': {
        // Chat metadata changed — update in-place
        const chatId = data.chat_id as number
        chatsStore.updateChatMetadata(chatId, {
          title: data.title as string | undefined,
          description: data.description as string | undefined,
          avatar_url: data.avatar_url as string | undefined,
        })
        break
      }
      case 'chat_deleted': {
        const chatId = data.chat_id as number
        chatsStore.removeChatLocal(chatId)
        break
      }
      case 'chat_member_joined': {
        // Optionally refresh chat details if viewing this chat
        // The member count will update on next full load
        break
      }
      case 'chat_member_left': {
        const chatId = data.chat_id as number
        const userId = data.user_id as number
        // If the current user left, remove the chat locally
        if (userId === authStore.user?.id) {
          chatsStore.removeChatLocal(chatId)
        }
        break
      }
      case 'user_profile_updated': {
        // Update cached display name / avatar in DM chats
        const userId = data.user_id as number
        const displayName = data.display_name as string | undefined
        const avatarUrl = data.avatar_url as string | undefined
        for (const chat of chatsStore.chats) {
          if (chat.type === 'direct' && (chat as Record<string, unknown>).other_user_id === userId) {
            if (displayName !== undefined) chat.title = displayName
            if (avatarUrl !== undefined) (chat as Record<string, unknown>).other_avatar_url = avatarUrl
          }
        }
        break
      }
    }
  }

  function startHeartbeat() {
    stopHeartbeat()
    pingTimer = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        const now = Date.now()
        const recentlyActive = now - lastUserActivity < 60_000
        const refreshDue = now - lastActivityRefreshSent > 120_000
        if (isPresenceActive && recentlyActive && refreshDue) {
          ws.send(JSON.stringify({ type: 'ping', active: true }))
          lastActivityRefreshSent = now
        } else {
          ws.send(JSON.stringify({ type: 'ping' }))
        }
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

  function startFallbackPolling() {
    if (fallbackPollTimer) return
    // Only start polling after 10 seconds of being disconnected
    fallbackPollTimer = setInterval(() => {
      if (disconnectedSince && Date.now() - disconnectedSince > 10000) {
        const chatsStore = useChatsStore()
        chatsStore.loadChats()
        const messagesStore = useMessagesStore()
        if (chatsStore.activeChatId) {
          messagesStore.loadNewer(chatsStore.activeChatId)
        }
      }
    }, 5000)
  }

  function stopFallbackPolling() {
    if (fallbackPollTimer) {
      clearInterval(fallbackPollTimer)
      fallbackPollTimer = null
    }
  }

  function disconnect() {
    intentionalClose = true
    stopHeartbeat()
    stopFallbackPolling()
    teardownPresence()
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
