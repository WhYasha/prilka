<template>
  <div
    v-if="visible"
    class="context-menu"
    :style="{ left: x + 'px', top: y + 'px' }"
  >
    <button class="ctx-item" @click="handleFavorite">
      {{ chat?.is_favorite ? '\u2605 Unstar' : '\u2606 Star' }}
    </button>
    <button class="ctx-item" @click="handleMute">
      {{ chat?.is_muted ? 'Unmute' : 'Mute' }}
    </button>
    <button
      v-if="chat && chat.type !== 'direct'"
      class="ctx-item ctx-leave"
      @click="handleLeave"
    >
      Leave chat
    </button>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useChatsStore } from '@/stores/chats'
import { useToast } from '@/composables/useToast'

const chatsStore = useChatsStore()
const { showToast } = useToast()

const visible = ref(false)
const x = ref(0)
const y = ref(0)
const chatId = ref<number | null>(null)

const chat = computed(() =>
  chatId.value ? chatsStore.chats.find((c) => c.id === chatId.value) ?? null : null,
)

function onShowContextMenu(e: CustomEvent) {
  chatId.value = e.detail.chatId
  x.value = e.detail.x
  y.value = e.detail.y
  visible.value = true
}

function onClickOutside() {
  if (visible.value) {
    visible.value = false
    chatId.value = null
  }
}

onMounted(() => {
  window.addEventListener('show-context-menu', onShowContextMenu as EventListener)
  document.addEventListener('click', onClickOutside)
})

onUnmounted(() => {
  window.removeEventListener('show-context-menu', onShowContextMenu as EventListener)
  document.removeEventListener('click', onClickOutside)
})

async function handleFavorite() {
  if (!chatId.value) return
  visible.value = false
  try {
    await chatsStore.toggleFavorite(chatId.value)
  } catch {
    showToast('Failed to update favorite')
  }
}

async function handleMute() {
  if (!chatId.value) return
  visible.value = false
  try {
    await chatsStore.toggleMute(chatId.value)
  } catch {
    showToast('Failed to update mute')
  }
}

async function handleLeave() {
  if (!chatId.value) return
  visible.value = false
  if (!confirm('Leave this chat?')) return
  try {
    await chatsStore.leave(chatId.value)
    showToast('Left chat')
  } catch {
    showToast('Failed to leave chat')
  }
}
</script>
