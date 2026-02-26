import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const useSelectionStore = defineStore('selection', () => {
  const selectedMessageIds = ref<Set<number>>(new Set())
  const selectionMode = ref(false)
  const currentChatId = ref<number | null>(null)

  const selectedCount = computed(() => selectedMessageIds.value.size)

  function enterSelectionMode(chatId: number, messageId?: number) {
    selectionMode.value = true
    currentChatId.value = chatId
    selectedMessageIds.value = new Set(messageId != null ? [messageId] : [])
  }

  function exitSelectionMode() {
    selectionMode.value = false
    currentChatId.value = null
    selectedMessageIds.value = new Set()
  }

  function toggleMessage(messageId: number) {
    const next = new Set(selectedMessageIds.value)
    if (next.has(messageId)) {
      next.delete(messageId)
    } else {
      next.add(messageId)
    }
    selectedMessageIds.value = next

    // Auto-exit if nothing selected
    if (next.size === 0) {
      exitSelectionMode()
    }
  }

  function addMessage(messageId: number) {
    if (!selectionMode.value) return
    if (selectedMessageIds.value.has(messageId)) return
    const next = new Set(selectedMessageIds.value)
    next.add(messageId)
    selectedMessageIds.value = next
  }

  function clearSelection() {
    selectedMessageIds.value = new Set()
  }

  function isSelected(messageId: number): boolean {
    return selectedMessageIds.value.has(messageId)
  }

  return {
    selectedMessageIds,
    selectionMode,
    currentChatId,
    selectedCount,
    enterSelectionMode,
    exitSelectionMode,
    toggleMessage,
    addMessage,
    clearSelection,
    isSelected,
  }
})
