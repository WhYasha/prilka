<template>
  <div class="layout" :class="{ 'chat-open': !!chatsStore.activeChatId }">
    <!-- Drawer -->
    <Drawer
      :open="drawerOpen"
      @close="drawerOpen = false"
      @open-profile="openProfileModal"
      @open-settings="openSettingsModal"
      @open-new-group="handleOpenNewGroup"
      @open-new-channel="handleOpenNewChannel"
      @open-favorites="handleOpenFavorites"
      @open-download="downloadModalOpen = true"
    />
    <div
      class="drawer-overlay"
      :class="{ visible: drawerOpen }"
      @click="drawerOpen = false"
    />

    <!-- Sidebar -->
    <Sidebar
      @open-drawer="drawerOpen = true"
      @open-new-chat="newChatInitialStep = 'select'; newChatModalOpen = true"
      @select-chat="handleSelectChat"
    />

    <!-- Chat Panel -->
    <ChatPanel
      @back="handleBack"
      @open-user-profile="openUserProfileModal"
    />

    <!-- Modals -->
    <NewChatModal
      v-if="newChatModalOpen"
      :initial-step="newChatInitialStep"
      @close="newChatModalOpen = false"
      @chat-created="handleChatCreated"
    />
    <ProfileModal
      v-if="profileModalOpen"
      :initial-tab="profileInitialTab"
      @close="profileModalOpen = false"
    />
    <UserProfileModal
      v-if="userProfileTarget"
      :username="userProfileTarget"
      @close="userProfileTarget = null"
      @message="handleDMFromProfile"
    />
    <DownloadModal
      v-if="downloadModalOpen"
      @close="downloadModalOpen = false"
    />
    <InvitePreviewModal
      v-if="inviteToken"
      :token="inviteToken"
      @close="inviteToken = null"
      @joined="handleInviteJoined"
    />

    <!-- Context Menu -->
    <ContextMenu />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, watch, nextTick } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useChatsStore } from '@/stores/chats'
import { useSettingsStore } from '@/stores/settings'
import { useSelectionStore } from '@/stores/selection'
import { useWebSocket } from '@/composables/useWebSocket'
import { getUserByUsername, getUser } from '@/api/users'
import { createChat, getChatByName } from '@/api/chats'
import { listStickers } from '@/api/stickers'
import { useToast } from '@/composables/useToast'
import { provide } from 'vue'
import type { Sticker } from '@/api/types'

import Sidebar from '@/components/layout/Sidebar.vue'
import ChatPanel from '@/components/layout/ChatPanel.vue'
import Drawer from '@/components/layout/Drawer.vue'
import NewChatModal from '@/components/modals/NewChatModal.vue'
import ProfileModal from '@/components/modals/ProfileModal.vue'
import UserProfileModal from '@/components/modals/UserProfileModal.vue'
import DownloadModal from '@/components/modals/DownloadModal.vue'
import InvitePreviewModal from '@/components/modals/InvitePreviewModal.vue'
import ContextMenu from '@/components/chat/ContextMenu.vue'

const route = useRoute()
const router = useRouter()
const authStore = useAuthStore()
const chatsStore = useChatsStore()
const settingsStore = useSettingsStore()
const selectionStore = useSelectionStore()
const { showToast } = useToast()
const { connect, disconnect, sendTyping } = useWebSocket()

// Provide sendTyping to child components (ChatPanel → Composer)
provide('sendTyping', sendTyping)

const drawerOpen = ref(false)
const newChatModalOpen = ref(false)
const newChatInitialStep = ref<'select' | 'direct' | 'group' | 'channel'>('select')
const profileModalOpen = ref(false)
const profileInitialTab = ref<'profile' | 'settings'>('profile')
const downloadModalOpen = ref(false)
const userProfileTarget = ref<string | null>(null)
const inviteToken = ref<string | null>(null)
const stickers = ref<Sticker[]>([])

// Pending scroll-to message for deep links
const pendingScrollMessageId = ref<number | null>(null)

// Provide stickers and pending scroll message to child components
provide('stickers', stickers)
provide('pendingScrollMessageId', pendingScrollMessageId)

// Chat polling timer
let chatsPollTimer: ReturnType<typeof setInterval> | null = null

onMounted(async () => {
  // Load settings
  await settingsStore.loadSettings()

  // Load stickers
  try {
    stickers.value = await listStickers()
  } catch {
    // stickers optional
  }

  // Load chats
  await chatsStore.loadChats()

  // Connect WebSocket
  connect()

  // Start chat polling (fallback alongside WS)
  chatsPollTimer = setInterval(() => chatsStore.loadChats(), 5000)

  // Handle deep links
  handleDeepLink()
})

onUnmounted(() => {
  disconnect()
  if (chatsPollTimer) {
    clearInterval(chatsPollTimer)
    chatsPollTimer = null
  }
})

// Watch route changes for deep links
watch(() => route.path, () => {
  handleDeepLink()
})

// Clear selection mode on chat navigation
watch(() => chatsStore.activeChatId, () => {
  if (selectionStore.selectionMode) {
    selectionStore.exitSelectionMode()
  }
})

async function handleDeepLink() {
  const path = route.path

  if (path.startsWith('/@')) {
    const username = route.params.username as string
    if (username) userProfileTarget.value = username
  } else if (path.startsWith('/u/')) {
    const userId = route.params.id as string
    if (userId) {
      try {
        const user = await getUser(parseInt(userId))
        userProfileTarget.value = user.username
      } catch {
        showToast('User not found')
      }
    }
  } else if (path.startsWith('/dm/')) {
    const target = route.params.id as string
    if (target) await openDM(target)
  } else if (path.startsWith('/c/')) {
    const chatIdOrName = (route.params.chatIdOrName || route.params.name) as string
    const messageId = route.params.messageId ? parseInt(route.params.messageId as string) : null
    if (chatIdOrName) {
      const isNumeric = /^\d+$/.test(chatIdOrName)
      if (isNumeric) {
        const chatId = parseInt(chatIdOrName)
        if (messageId) {
          await openChatAndScrollToMessage(chatId, messageId)
        } else {
          chatsStore.setActiveChat(chatId)
        }
      } else {
        await openChannelByName(chatIdOrName)
      }
    }
  }
}

async function openDM(target: string) {
  try {
    let userId: number
    if (target.startsWith('@')) {
      const username = target.substring(1)
      if (authStore.user?.username === username) {
        userProfileTarget.value = username
        return
      }
      const user = await getUserByUsername(username)
      userId = user.id
    } else {
      userId = parseInt(target)
      if (authStore.user?.id === userId) {
        showToast('Cannot open DM with yourself')
        return
      }
    }
    const chat = await createChat({ type: 'direct', member_ids: [userId] })
    await chatsStore.loadChats()
    chatsStore.setActiveChat(chat.id)
  } catch {
    showToast('Failed to open conversation')
  }
}

async function openChannelByName(name: string) {
  try {
    const chat = await getChatByName(name)
    const existing = chatsStore.chats.find((c) => c.id === chat.id)
    if (existing) {
      chatsStore.setActiveChat(chat.id)
    } else {
      showToast('You are not a member of this channel')
    }
  } catch {
    showToast('Channel not found')
  }
}

async function openChatAndScrollToMessage(chatId: number, messageId: number) {
  const existing = chatsStore.chats.find((c) => c.id === chatId)
  if (!existing) {
    showToast('Chat not found')
    return
  }
  chatsStore.setActiveChat(chatId)
  await nextTick()
  pendingScrollMessageId.value = messageId
}

function handleSelectChat(chatId: number) {
  chatsStore.setActiveChat(chatId)
  router.replace('/app')
}

function handleBack() {
  chatsStore.setActiveChat(null)
}

function openProfileModal() {
  drawerOpen.value = false
  profileInitialTab.value = 'profile'
  profileModalOpen.value = true
}

function openSettingsModal() {
  drawerOpen.value = false
  profileInitialTab.value = 'settings'
  profileModalOpen.value = true
}

function handleOpenNewGroup() {
  drawerOpen.value = false
  newChatInitialStep.value = 'group'
  newChatModalOpen.value = true
}

function handleOpenNewChannel() {
  drawerOpen.value = false
  newChatInitialStep.value = 'channel'
  newChatModalOpen.value = true
}

function handleOpenFavorites() {
  drawerOpen.value = false
}

function openUserProfileModal(username: string) {
  userProfileTarget.value = username
}

async function handleDMFromProfile(username: string) {
  userProfileTarget.value = null
  await openDM('@' + username)
}

async function handleChatCreated(chatId: number) {
  newChatModalOpen.value = false
  await chatsStore.loadChats()
  chatsStore.setActiveChat(chatId)
}

async function handleInviteJoined(chatId: number) {
  inviteToken.value = null
  await chatsStore.loadChats()
  chatsStore.setActiveChat(chatId)
}

// Intercept clicks on invite links within the messenger
function handleInviteLinkClick(e: MouseEvent) {
  const target = (e.target as HTMLElement)?.closest('a')
  if (!target) return
  const href = target.getAttribute('href')
  if (!href) return

  // Match /join/{token} paths (relative or absolute with same origin)
  let match: RegExpMatchArray | null = null
  try {
    const url = new URL(href, window.location.origin)
    if (url.origin === window.location.origin) {
      match = url.pathname.match(/^\/join\/([^/]+)$/)
    }
  } catch {
    match = href.match(/^\/join\/([^/]+)$/)
  }

  if (match) {
    e.preventDefault()
    e.stopPropagation()
    inviteToken.value = match[1] ?? null
  }
}

// Global ESC handler
function handleEsc(e: KeyboardEvent) {
  if (e.key !== 'Escape') return
  if (inviteToken.value) { inviteToken.value = null; return }
  if (selectionStore.selectionMode) { selectionStore.exitSelectionMode(); return }
  if (userProfileTarget.value) { userProfileTarget.value = null; return }
  if (newChatModalOpen.value) { newChatModalOpen.value = false; return }
  if (profileModalOpen.value) { profileModalOpen.value = false; return }
  if (downloadModalOpen.value) { downloadModalOpen.value = false; return }
  if (drawerOpen.value) { drawerOpen.value = false; return }
  if (chatsStore.activeChatId !== null) { handleBack(); return }
}

onMounted(() => {
  document.addEventListener('keydown', handleEsc)
  document.addEventListener('click', handleInviteLinkClick, true)
})
onUnmounted(() => {
  document.removeEventListener('keydown', handleEsc)
  document.removeEventListener('click', handleInviteLinkClick, true)
})
</script>
