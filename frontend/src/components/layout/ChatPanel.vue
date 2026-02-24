<template>
  <main class="chat-panel">
    <!-- Empty state when no chat selected -->
    <div v-if="!chatsStore.activeChatId" class="empty-chat">
      <div class="empty-chat-icon">&#128172;</div>
      <div class="empty-chat-text">Select a chat to start messaging</div>
    </div>

    <!-- Active chat view -->
    <div v-else class="chat-view">
      <!-- Selection bar (multi-select mode) -->
      <SelectionBar
        @forward="onSelectionForward"
        @delete="onSelectionDelete"
      />

      <ChatHeader
        v-show="!selectionStore.selectionMode"
        @back="emit('back')"
        @open-user-profile="(u) => emit('openUserProfile', u)"
        @open-channel-info="channelInfoModalOpen = true"
      />

      <!-- Pinned message bar -->
      <PinBar
        v-if="pinnedMessage && !selectionStore.selectionMode"
        :message="pinnedMessage.message"
        @click="scrollToMessage(pinnedMessage!.message.id)"
        @dismiss="messagesStore.dismissPin(chatsStore.activeChatId!)"
      />

      <!-- Channel read-only bar -->
      <div v-if="isChannelReadonly" class="channel-readonly-bar">
        <span class="readonly-icon">&#128274;</span>
        <span class="readonly-text">Only admins can send messages in this channel.</span>
      </div>

      <!-- Messages -->
      <div ref="msgListRef" class="msg-list" @click="closeEmojiPicker">
        <Spinner v-if="messagesStore.loadingChat === chatsStore.activeChatId" />
        <template v-else>
          <!-- Sentinel for loading older messages -->
          <div ref="olderSentinelRef" class="older-sentinel">
            <Spinner v-if="loadingOlder" />
          </div>
          <template v-for="item in messagesWithDates" :key="item.type === 'date' ? 'date-' + item.label : 'msg-' + item.msg!.id">
            <div v-if="item.type === 'date'" class="date-sep">
              <span>{{ item.label }}</span>
            </div>
            <MessageBubble
              v-else
              :message="item.msg!"
              :is-mine="item.msg!.sender_id === authStore.user?.id"
              @mention-click="(u) => emit('openUserProfile', u)"
              @open-emoji-picker="onOpenEmojiPicker"
              @toggle-reaction="onToggleReaction"
              @reply-click="scrollToMessage"
            />
          </template>
        </template>
      </div>

      <!-- Emoji reaction picker -->
      <EmojiPicker
        :visible="emojiPickerVisible"
        :x="emojiPickerX"
        :y="emojiPickerY"
        @select="onEmojiSelect"
        @close="emojiPickerVisible = false"
      />

      <!-- Sticker picker -->
      <div v-if="stickerPickerOpen" class="sticker-picker">
        <StickerPicker @select="handleSendSticker" />
      </div>

      <!-- Composer (hidden for readonly channels) -->
      <Composer
        v-if="!isChannelReadonly"
        v-show="!isRecording"
        :sticker-picker-open="stickerPickerOpen"
        :reply-to="replyToMessage"
        @send="handleSendText"
        @edit-message="handleEditMessage"
        @toggle-stickers="stickerPickerOpen = !stickerPickerOpen"
        @start-recording="startRec"
        @typing="handleTyping"
      />

      <!-- Recording bar -->
      <div v-if="isRecording" class="recording-bar">
        <span class="rec-dot" />
        <span class="rec-time">{{ recorderComposable.formatTime(recorderComposable.seconds.value) }}</span>
        <span class="rec-label">Recording... tap mic to stop</span>
        <button class="icon-btn rec-cancel" title="Cancel" @click="cancelRec">&#10005;</button>
      </div>

      <!-- Invite management -->
      <InviteSection v-if="showInviteSection" :chat-id="chatsStore.activeChatId!" />

      <!-- Message context menu (desktop right-click) -->
      <MessageContextMenu />

      <!-- Bottom sheet (mobile long-press) -->
      <BottomSheet :visible="bottomSheetVisible" @close="bottomSheetVisible = false">
        <button class="ctx-item" @click="bottomSheetAction('reply')">Reply</button>
        <button v-if="isBottomSheetEditableMessage" class="ctx-item" @click="bottomSheetAction('edit')">Edit</button>
        <button class="ctx-item" @click="bottomSheetAction(isBottomSheetPinnedMessage ? 'unpin' : 'pin')">
          {{ isBottomSheetPinnedMessage ? 'Unpin' : 'Pin' }}
        </button>
        <button class="ctx-item" @click="bottomSheetAction('copy')">Copy text</button>
        <button class="ctx-item" @click="bottomSheetAction('copyLink')">Copy link</button>
        <button class="ctx-item" @click="bottomSheetAction('forward')">Forward</button>
        <button class="ctx-item" @click="bottomSheetAction('select')">Select</button>
        <button class="ctx-item ctx-delete" @click="bottomSheetAction('delete')">Delete</button>
      </BottomSheet>

      <!-- Forward dialog -->
      <ForwardDialog
        v-if="forwardDialogVisible"
        :message-ids="forwardMessageIds"
        :from-chat-id="chatsStore.activeChatId!"
        @close="closeForwardDialog"
        @forwarded="closeForwardDialog"
      />

      <!-- Delete confirmation modal -->
      <DeleteConfirmModal
        v-if="deleteModalVisible"
        :message-count="deleteMessageIds.length"
        :can-delete-for-everyone="canDeleteForEveryone"
        @close="deleteModalVisible = false"
        @confirm="onDeleteConfirm"
      />

      <!-- Channel info modal -->
      <ChannelInfoModal
        v-if="channelInfoModalOpen"
        :chat-id="String(chatsStore.activeChatId)"
        @close="channelInfoModalOpen = false"
      />
    </div>
  </main>
</template>

<script setup lang="ts">
import { ref, computed, watch, nextTick, inject, onMounted, onUnmounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useChatsStore } from '@/stores/chats'
import { useMessagesStore } from '@/stores/messages'
import { useRecorder } from '@/composables/useRecorder'
import { useToast } from '@/composables/useToast'
import { getChat } from '@/api/chats'
import { uploadFile } from '@/api/files'
import * as messagesApi from '@/api/messages'
import type { Message, Sticker } from '@/api/types'

import PinBar from '@/components/chat/PinBar.vue'
import ChatHeader from '@/components/chat/ChatHeader.vue'
import MessageBubble from '@/components/chat/MessageBubble.vue'
import Composer from '@/components/chat/Composer.vue'
import StickerPicker from '@/components/chat/StickerPicker.vue'
import Spinner from '@/components/ui/Spinner.vue'
import InviteSection from '@/components/chat/InviteSection.vue'
import EmojiPicker from '@/components/chat/EmojiPicker.vue'
import MessageContextMenu from '@/components/chat/MessageContextMenu.vue'
import SelectionBar from '@/components/chat/SelectionBar.vue'
import BottomSheet from '@/components/ui/BottomSheet.vue'
import ForwardDialog from '@/components/modals/ForwardDialog.vue'
import DeleteConfirmModal from '@/components/modals/DeleteConfirmModal.vue'
import ChannelInfoModal from '@/components/chat/ChannelInfoModal.vue'
import { useSelectionStore } from '@/stores/selection'

const emit = defineEmits<{
  back: []
  openUserProfile: [username: string]
}>()

const authStore = useAuthStore()
const chatsStore = useChatsStore()
const messagesStore = useMessagesStore()
const { showToast } = useToast()
const recorderComposable = useRecorder()
const stickers = inject<{ value: Sticker[] }>('stickers', ref([]))
const sendTyping = inject<(chatId: number) => void>('sendTyping', () => {})
const pendingScrollMessageId = inject<{ value: number | null }>('pendingScrollMessageId', ref(null))

const msgListRef = ref<HTMLElement | null>(null)
const olderSentinelRef = ref<HTMLElement | null>(null)
const loadingOlder = ref(false)
let olderObserver: IntersectionObserver | null = null
const stickerPickerOpen = ref(false)
const isRecording = ref(false)
const myRole = ref<string>('member')
const showInviteSection = ref(false)
const channelInfoModalOpen = ref(false)

// Emoji picker state
const emojiPickerVisible = ref(false)
const emojiPickerX = ref(0)
const emojiPickerY = ref(0)
const emojiPickerMessageId = ref<number | null>(null)

const selectionStore = useSelectionStore()

// Bottom sheet state
const bottomSheetVisible = ref(false)
const bottomSheetMessageId = ref<number | null>(null)
const bottomSheetMessageText = ref('')
const isBottomSheetOwnMessage = computed(() => {
  if (!bottomSheetMessageId.value || !authStore.user) return false
  const msg = messages.value.find((m) => m.id === bottomSheetMessageId.value)
  return msg?.sender_id === authStore.user.id
})

const isBottomSheetEditableMessage = computed(() => {
  if (!isBottomSheetOwnMessage.value) return false
  const msg = messages.value.find((m) => m.id === bottomSheetMessageId.value)
  return msg?.message_type === 'text'
})

const isBottomSheetPinnedMessage = computed(() => {
  if (!bottomSheetMessageId.value || !chatsStore.activeChatId) return false
  const pinned = messagesStore.pinnedByChat[chatsStore.activeChatId]
  return pinned?.message?.id === bottomSheetMessageId.value
})

const pinnedMessage = computed(() => {
  if (!chatsStore.activeChatId) return null
  if (messagesStore.pinnedDismissed.has(chatsStore.activeChatId)) return null
  return messagesStore.pinnedByChat[chatsStore.activeChatId] || null
})

// Forward dialog state
const forwardDialogVisible = ref(false)
const forwardMessageIds = ref<number[]>([])

// Delete modal state
const deleteModalVisible = ref(false)
const deleteMessageIds = ref<number[]>([])
const canDeleteForEveryone = ref(false)

// Reply state
const replyToMessage = ref<Message | null>(null)

// Polling timer for messages
let msgPollTimer: ReturnType<typeof setInterval> | null = null

const isChannelReadonly = computed(() => {
  const chat = chatsStore.activeChat
  return chat?.type === 'channel' && myRole.value === 'member'
})

const messages = computed(() => {
  if (!chatsStore.activeChatId) return []
  return messagesStore.getMessages(chatsStore.activeChatId)
})

interface DateItem { type: 'date'; label: string; msg?: undefined }
interface MsgItem { type: 'msg'; msg: typeof messages.value[0]; label?: undefined }

const messagesWithDates = computed<(DateItem | MsgItem)[]>(() => {
  const items: (DateItem | MsgItem)[] = []
  let lastDateLabel = ''
  for (const m of messages.value) {
    const dl = formatDateLabel(m.created_at)
    if (dl !== lastDateLabel) {
      items.push({ type: 'date', label: dl })
      lastDateLabel = dl
    }
    items.push({ type: 'msg', msg: m })
  }
  return items
})

function formatDateLabel(isoStr: string): string {
  if (!isoStr) return ''
  const d = new Date(isoStr.endsWith('Z') ? isoStr : isoStr + 'Z')
  const now = new Date()
  if (d.toDateString() === now.toDateString()) return 'Today'
  const yesterday = new Date(now)
  yesterday.setDate(now.getDate() - 1)
  if (d.toDateString() === yesterday.toDateString()) return 'Yesterday'
  return d.toLocaleDateString([], { year: 'numeric', month: 'long', day: 'numeric' })
}

// --- IntersectionObserver for loading older messages ---
function setupOlderObserver() {
  teardownOlderObserver()
  if (!olderSentinelRef.value || !msgListRef.value) return
  olderObserver = new IntersectionObserver(
    async (entries) => {
      const entry = entries[0]
      if (!entry?.isIntersecting) return
      const chatId = chatsStore.activeChatId
      if (!chatId || loadingOlder.value) return
      if (messagesStore.loadingChat === chatId) return

      loadingOlder.value = true
      const el = msgListRef.value
      const oldScrollHeight = el ? el.scrollHeight : 0

      try {
        const older = await messagesStore.loadOlder(chatId)
        if (older.length > 0 && el) {
          await nextTick()
          const newScrollHeight = el.scrollHeight
          el.scrollTop += newScrollHeight - oldScrollHeight
        }
      } finally {
        loadingOlder.value = false
      }
    },
    { root: msgListRef.value, threshold: 0 },
  )
  olderObserver.observe(olderSentinelRef.value)
}

function teardownOlderObserver() {
  if (olderObserver) {
    olderObserver.disconnect()
    olderObserver = null
  }
}

// Watch active chat changes
watch(
  () => chatsStore.activeChatId,
  async (chatId) => {
    stickerPickerOpen.value = false
    myRole.value = 'member'
    showInviteSection.value = false
    teardownOlderObserver()

    // Stop previous polling
    if (msgPollTimer) {
      clearInterval(msgPollTimer)
      msgPollTimer = null
    }

    if (!chatId) return

    // Load messages and pinned message
    await Promise.all([
      messagesStore.loadMessages(chatId),
      messagesStore.loadPinnedMessage(chatId),
    ])
    await nextTick()
    scrollToBottom()

    // Setup IntersectionObserver for older messages after DOM is ready
    await nextTick()
    setupOlderObserver()

    // Check role for channels/groups
    const chat = chatsStore.activeChat
    if (chat && (chat.type === 'channel' || chat.type === 'group')) {
      try {
        const detail = await getChat(chatId)
        myRole.value = detail.my_role || 'member'
        showInviteSection.value = myRole.value === 'owner' || myRole.value === 'admin'
      } catch {
        // ignore
      }
    }

    // Start message polling (fallback — don't interfere with scroll position)
    msgPollTimer = setInterval(() => {
      if (chatsStore.activeChatId === chatId) {
        messagesStore.loadNewer(chatId).then((msgs) => {
          if (msgs.length > 0) scrollToBottomIfNear()
        })
      }
    }, 2500)
  },
)

function scrollToBottom() {
  if (msgListRef.value) {
    msgListRef.value.scrollTop = msgListRef.value.scrollHeight
  }
}

function scrollToBottomIfNear() {
  if (!msgListRef.value) return
  const el = msgListRef.value
  const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 80
  if (atBottom) {
    nextTick(() => {
      el.scrollTop = el.scrollHeight
    })
  }
}

function handleTyping() {
  if (chatsStore.activeChatId) {
    sendTyping(chatsStore.activeChatId)
  }
}

async function handleSendText(content: string, replyTo?: { messageId: number; senderName: string; snippet: string }) {
  if (!chatsStore.activeChatId || !content.trim()) return
  replyToMessage.value = null
  try {
    // Include reply context if present (content-only for now; backend reply_to may need future work)
    const extra = replyTo ? { reply_to_message_id: replyTo.messageId } : undefined
    const msg = await messagesStore.sendMessage(chatsStore.activeChatId, content.trim(), 'text', extra)
    // Enrich locally
    if (authStore.user) {
      msg.sender_id = authStore.user.id
      msg.sender_username = authStore.user.username
      msg.sender_display_name = authStore.user.display_name
    }
    await nextTick()
    scrollToBottom()
    chatsStore.loadChats()
  } catch {
    showToast('Failed to send message')
  }
}

async function handleEditMessage(messageId: number, content: string) {
  if (!chatsStore.activeChatId || !content.trim()) return
  try {
    await messagesStore.editMessage(chatsStore.activeChatId, messageId, content.trim())
  } catch {
    showToast('Failed to edit message')
  }
}

async function handleSendSticker(stickerId: number) {
  if (!chatsStore.activeChatId) return
  stickerPickerOpen.value = false
  try {
    const msg = await messagesStore.sendMessage(chatsStore.activeChatId, '', 'sticker', {
      sticker_id: stickerId,
    })
    // Enrich with local sticker data
    const localSticker = stickers.value.find((s: Sticker) => s.id === stickerId)
    if (localSticker) {
      msg.sticker_url = localSticker.url
      msg.sticker_label = localSticker.label
    }
    await nextTick()
    scrollToBottom()
    chatsStore.loadChats()
  } catch {
    showToast('Failed to send sticker')
  }
}

function onOpenEmojiPicker(messageId: number, x: number, y: number) {
  emojiPickerMessageId.value = messageId
  emojiPickerX.value = x
  emojiPickerY.value = y
  emojiPickerVisible.value = true
}

function onEmojiSelect(emoji: string) {
  if (emojiPickerMessageId.value && chatsStore.activeChatId) {
    messagesStore.toggleReaction(chatsStore.activeChatId, emojiPickerMessageId.value, emoji)
  }
  emojiPickerVisible.value = false
  emojiPickerMessageId.value = null
}

function onToggleReaction(messageId: number, emoji: string) {
  if (chatsStore.activeChatId) {
    messagesStore.toggleReaction(chatsStore.activeChatId, messageId, emoji)
  }
}

function closeEmojiPicker() {
  emojiPickerVisible.value = false
  emojiPickerMessageId.value = null
}

async function startRec() {
  try {
    await recorderComposable.startRecording()
    isRecording.value = true

    // Wait for stop
    const file = await recorderComposable.stopRecording()
    isRecording.value = false
    await uploadVoice(file)
  } catch {
    isRecording.value = false
    showToast('Microphone access denied')
  }
}

function cancelRec() {
  recorderComposable.cancelRecording()
  isRecording.value = false
}

// --- Message interaction handlers ---

function onForwardMessages(e: CustomEvent) {
  const { messageIds } = e.detail
  forwardMessageIds.value = messageIds
  forwardDialogVisible.value = true
}

function onSelectMessage(e: CustomEvent) {
  const { messageId, chatId } = e.detail
  if (chatId) {
    selectionStore.enterSelectionMode(chatId, messageId)
  }
}

async function handlePinMessage(chatId: number, messageId: number) {
  try {
    await messagesStore.pinMsg(chatId, messageId)
  } catch {
    showToast('Failed to pin message')
  }
}

async function handleUnpinMessage(chatId: number, messageId: number) {
  try {
    await messagesStore.unpinMsg(chatId, messageId)
  } catch {
    showToast('Failed to unpin message')
  }
}

function onPinMessage(e: CustomEvent) {
  const { messageId, chatId } = e.detail
  if (messageId && chatId) handlePinMessage(chatId, messageId)
}

function onUnpinMessage(e: CustomEvent) {
  const { messageId, chatId } = e.detail
  if (messageId && chatId) handleUnpinMessage(chatId, messageId)
}

function onDeleteMessage(e: CustomEvent) {
  const { messageId, chatId } = e.detail
  if (!messageId || !chatId) return
  deleteMessageIds.value = [messageId]
  // Check if user can delete for everyone (own message within 48h)
  const msg = messages.value.find((m) => m.id === messageId)
  canDeleteForEveryone.value = !!(msg && msg.sender_id === authStore.user?.id)
  deleteModalVisible.value = true
}

function onSelectionForward() {
  forwardMessageIds.value = [...selectionStore.selectedMessageIds]
  forwardDialogVisible.value = true
}

function onSelectionDelete() {
  deleteMessageIds.value = [...selectionStore.selectedMessageIds]
  // Can delete for everyone only if all selected messages are mine
  canDeleteForEveryone.value = deleteMessageIds.value.every((id) => {
    const msg = messages.value.find((m) => m.id === id)
    return msg && msg.sender_id === authStore.user?.id
  })
  deleteModalVisible.value = true
}

function closeForwardDialog() {
  forwardDialogVisible.value = false
  forwardMessageIds.value = []
  selectionStore.exitSelectionMode()
}

async function onDeleteConfirm(_forEveryone: boolean) {
  deleteModalVisible.value = false
  const chatId = chatsStore.activeChatId
  if (!chatId) return
  for (const msgId of deleteMessageIds.value) {
    try {
      await messagesStore.deleteMessage(chatId, msgId)
    } catch {
      showToast('Failed to delete message')
    }
  }
  deleteMessageIds.value = []
  selectionStore.exitSelectionMode()
}

function bottomSheetAction(action: string) {
  bottomSheetVisible.value = false
  const msgId = bottomSheetMessageId.value
  const chatId = chatsStore.activeChatId
  if (!msgId || !chatId) return

  switch (action) {
    case 'reply': {
      const msg = messages.value.find((m) => m.id === msgId)
      window.dispatchEvent(new CustomEvent('reply-to-message', {
        detail: {
          messageId: msgId,
          senderName: msg?.sender_display_name || msg?.sender_username || 'Unknown',
          text: msg?.content || '',
        },
      }))
      break
    }
    case 'edit': {
      window.dispatchEvent(new CustomEvent('edit-message', {
        detail: {
          messageId: msgId,
          chatId,
          text: bottomSheetMessageText.value,
        },
      }))
      break
    }
    case 'copy':
      navigator.clipboard.writeText(bottomSheetMessageText.value).then(
        () => showToast('Copied to clipboard'),
        () => showToast('Failed to copy text'),
      )
      break
    case 'copyLink':
      navigator.clipboard.writeText(`${window.location.origin}/c/${chatId}/${msgId}`).then(
        () => showToast('Link copied'),
        () => showToast('Failed to copy link'),
      )
      break
    case 'forward':
      forwardMessageIds.value = [msgId]
      forwardDialogVisible.value = true
      break
    case 'select':
      selectionStore.enterSelectionMode(chatId, msgId)
      break
    case 'delete':
      onDeleteMessage(new CustomEvent('', { detail: { messageId: msgId, chatId } }))
      break
    case 'pin':
      handlePinMessage(chatId, msgId)
      break
    case 'unpin':
      handleUnpinMessage(chatId, msgId)
      break
  }
}

async function scrollToMessage(messageId: number) {
  if (!msgListRef.value) return
  const chatId = chatsStore.activeChatId
  if (!chatId) return

  // Try to find the message element in DOM
  let el = msgListRef.value.querySelector(`[data-message-id="${messageId}"]`) as HTMLElement | null

  if (!el) {
    // Message not loaded yet — fetch messages around the target ID
    try {
      const afterId = Math.max(0, messageId - 25)
      const msgs = await messagesApi.listMessages(chatId, {
        after_id: afterId,
        limit: 50,
      })
      // Check if target message exists in fetched results
      const found = msgs.find((m) => m.id === messageId)
      if (!found) {
        showToast('Message not found')
        return
      }
      // Replace loaded messages with the fetched range
      messagesStore.messagesByChat[chatId] = msgs
      if (msgs.length > 0) {
        messagesStore.lastMsgId[chatId] = Math.max(...msgs.map((m) => m.id))
      }
      await nextTick()
      el = msgListRef.value?.querySelector(`[data-message-id="${messageId}"]`) as HTMLElement | null
    } catch {
      showToast('Message not found')
      return
    }
  }

  if (el) {
    el.scrollIntoView({ behavior: 'smooth', block: 'center' })
    el.classList.add('highlight-message')
    setTimeout(() => el!.classList.remove('highlight-message'), 2000)
  }
}

// ESC handler for channel info modal
function handleEsc(e: KeyboardEvent) {
  if (e.key !== 'Escape') return
  if (channelInfoModalOpen.value) {
    channelInfoModalOpen.value = false
    e.stopPropagation()
  }
}

// Listen for custom events from MessageContextMenu and other components
onMounted(() => {
  window.addEventListener('forward-messages', onForwardMessages as EventListener)
  window.addEventListener('select-message', onSelectMessage as EventListener)
  window.addEventListener('delete-message', onDeleteMessage as EventListener)
  window.addEventListener('pin-message', onPinMessage as EventListener)
  window.addEventListener('unpin-message', onUnpinMessage as EventListener)
  document.addEventListener('keydown', handleEsc)
})

onUnmounted(() => {
  teardownOlderObserver()
  window.removeEventListener('forward-messages', onForwardMessages as EventListener)
  window.removeEventListener('select-message', onSelectMessage as EventListener)
  window.removeEventListener('delete-message', onDeleteMessage as EventListener)
  window.removeEventListener('pin-message', onPinMessage as EventListener)
  window.removeEventListener('unpin-message', onUnpinMessage as EventListener)
  document.removeEventListener('keydown', handleEsc)
})

// Handle pending scroll-to message from deep links
watch(
  () => pendingScrollMessageId.value,
  async (messageId) => {
    if (!messageId) return
    await nextTick()
    // Small delay to ensure messages are rendered
    setTimeout(() => {
      scrollToMessage(messageId)
      pendingScrollMessageId.value = null
    }, 300)
  },
)

// Clear selection and reply when chat changes
watch(
  () => chatsStore.activeChatId,
  () => {
    selectionStore.exitSelectionMode()
    replyToMessage.value = null
  },
)

async function uploadVoice(file: File) {
  if (!chatsStore.activeChatId) return
  try {
    const fileData = await uploadFile(file)
    await messagesStore.sendMessage(chatsStore.activeChatId, '', 'voice', {
      file_id: fileData.id,
      duration_seconds: recorderComposable.seconds.value,
    })
    await messagesStore.loadMessages(chatsStore.activeChatId)
    await nextTick()
    scrollToBottom()
    chatsStore.loadChats()
  } catch {
    showToast('Failed to upload voice message')
  }
}
</script>
