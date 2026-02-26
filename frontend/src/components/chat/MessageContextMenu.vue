<template>
  <Teleport to="body">
    <div
      v-if="visible"
      class="msg-context-menu"
      :style="{ left: posX + 'px', top: posY + 'px' }"
      @click.stop
      @contextmenu.prevent
    >
      <template v-if="areaOnly">
        <button class="ctx-item" @click="handleSelectMessages">Select Messages</button>
      </template>
      <template v-else>
        <button class="ctx-item" @click="handleReply">
          Reply
        </button>
        <button v-if="isOwnMessage && messageType === 'text'" class="ctx-item" @click="handleEdit">
          Edit
        </button>
        <button class="ctx-item" @click="handlePin">
          {{ isPinnedMessage ? 'Unpin' : 'Pin' }}
        </button>
        <button class="ctx-item" @click="handleCopyText">
          Copy text
        </button>
        <button v-if="messageLink" class="ctx-item" @click="handleCopyLink">
          Copy link
        </button>
        <button class="ctx-item" @click="handleForward">
          Forward
        </button>
        <button class="ctx-item" @click="handleSelect">
          Select
        </button>
        <button class="ctx-item ctx-delete" @click="handleDelete">
          Delete
        </button>
      </template>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useToast } from '@/composables/useToast'
import { useAuthStore } from '@/stores/auth'
import { useMessagesStore } from '@/stores/messages'
import { useSelectionStore } from '@/stores/selection'

const { showToast } = useToast()
const authStore = useAuthStore()
const messagesStore = useMessagesStore()
const selectionStore = useSelectionStore()

const visible = ref(false)
const posX = ref(0)
const posY = ref(0)
const messageId = ref<number | null>(null)
const chatId = ref<number | null>(null)
const messageText = ref('')
const messageType = ref('')
const senderName = ref('')
const senderId = ref<number | null>(null)
const areaOnly = ref(false)

const isOwnMessage = computed(() => {
  if (!senderId.value || !authStore.user) return false
  return senderId.value === authStore.user.id
})

const messageLink = computed(() => {
  if (!chatId.value || !messageId.value) return null
  return `${window.location.origin}/c/${chatId.value}/${messageId.value}`
})

const isPinnedMessage = computed(() => {
  if (!chatId.value || !messageId.value) return false
  const pinned = messagesStore.pinnedByChat[chatId.value]
  return pinned?.message?.id === messageId.value
})

function onShowMessageContextMenu(e: CustomEvent) {
  areaOnly.value = !!e.detail.areaOnly
  messageId.value = e.detail.messageId ?? null
  chatId.value = e.detail.chatId
  messageText.value = e.detail.text || ''
  messageType.value = e.detail.messageType || 'text'
  senderName.value = e.detail.senderName || ''
  senderId.value = e.detail.senderId ?? null
  posX.value = e.detail.x
  posY.value = e.detail.y
  visible.value = true

  // Clamp to viewport
  requestAnimationFrame(() => {
    const el = document.querySelector('.msg-context-menu') as HTMLElement | null
    if (!el) return
    const rect = el.getBoundingClientRect()
    if (rect.right > window.innerWidth) {
      posX.value = window.innerWidth - rect.width - 8
    }
    if (rect.bottom > window.innerHeight) {
      posY.value = window.innerHeight - rect.height - 8
    }
  })
}

function onClickOutside() {
  if (visible.value) {
    visible.value = false
    messageId.value = null
    chatId.value = null
    messageText.value = ''
    messageType.value = ''
    senderName.value = ''
    senderId.value = null
    areaOnly.value = false
  }
}

onMounted(() => {
  window.addEventListener('show-message-context-menu', onShowMessageContextMenu as EventListener)
  document.addEventListener('click', onClickOutside)
})

onUnmounted(() => {
  window.removeEventListener('show-message-context-menu', onShowMessageContextMenu as EventListener)
  document.removeEventListener('click', onClickOutside)
})

function hide() {
  visible.value = false
}

function handleReply() {
  hide()
  if (!messageId.value || !chatId.value) return
  window.dispatchEvent(
    new CustomEvent('reply-to-message', {
      detail: {
        messageId: messageId.value,
        chatId: chatId.value,
        senderName: senderName.value,
        text: messageText.value,
      },
    }),
  )
}

function handleEdit() {
  hide()
  if (!messageId.value || !chatId.value) return
  window.dispatchEvent(
    new CustomEvent('edit-message', {
      detail: {
        messageId: messageId.value,
        chatId: chatId.value,
        text: messageText.value,
      },
    }),
  )
}

async function handleCopyText() {
  hide()
  try {
    await navigator.clipboard.writeText(messageText.value)
    showToast('Copied to clipboard')
  } catch {
    showToast('Failed to copy text')
  }
}

async function handleCopyLink() {
  hide()
  if (!messageLink.value) return
  try {
    await navigator.clipboard.writeText(messageLink.value)
    showToast('Link copied')
  } catch {
    showToast('Failed to copy link')
  }
}

function handleForward() {
  hide()
  if (!messageId.value || !chatId.value) return
  window.dispatchEvent(
    new CustomEvent('forward-messages', {
      detail: { messageIds: [messageId.value], chatId: chatId.value },
    }),
  )
}

function handleSelect() {
  hide()
  if (!messageId.value || !chatId.value) return
  window.dispatchEvent(
    new CustomEvent('select-message', {
      detail: { messageId: messageId.value, chatId: chatId.value },
    }),
  )
}

function handlePin() {
  hide()
  if (!messageId.value || !chatId.value) return
  const eventName = isPinnedMessage.value ? 'unpin-message' : 'pin-message'
  window.dispatchEvent(
    new CustomEvent(eventName, {
      detail: { messageId: messageId.value, chatId: chatId.value },
    }),
  )
}

function handleSelectMessages() {
  hide()
  if (!chatId.value) return
  selectionStore.enterSelectionMode(chatId.value)
}

function handleDelete() {
  hide()
  if (!messageId.value || !chatId.value) return
  window.dispatchEvent(
    new CustomEvent('delete-message', {
      detail: { messageId: messageId.value, chatId: chatId.value },
    }),
  )
}
</script>

<style scoped>
.msg-context-menu {
  position: fixed;
  z-index: 9999;
  min-width: 180px;
  background: var(--ctx-menu-bg);
  border: 1px solid var(--ctx-menu-border);
  border-radius: 8px;
  box-shadow: var(--ctx-menu-shadow);
  padding: 4px 0;
  display: flex;
  flex-direction: column;
  animation: ctx-menu-in var(--transition-fast) ease;
}

.ctx-item {
  display: block;
  width: 100%;
  padding: 10px 16px;
  border: none;
  background: none;
  text-align: left;
  font-size: 14px;
  color: var(--ctx-menu-text);
  cursor: pointer;
  transition: background var(--transition-fast);
}

.ctx-item:hover:not(:disabled) {
  background: var(--ctx-menu-hover);
}

.ctx-item:disabled {
  opacity: 0.4;
  cursor: default;
}

.ctx-delete {
  color: var(--danger);
}
</style>
