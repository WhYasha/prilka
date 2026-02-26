import { ref } from 'vue'
import { useSelectionStore } from '@/stores/selection'

export interface UseDragSelectOptions {
  chatId: () => number | null
  verticalThreshold?: number
}

export function useDragSelect(options: UseDragSelectOptions) {
  const {
    chatId,
    verticalThreshold = 8,
  } = options

  const isDragSelecting = ref(false)
  let startY = 0
  let active = false
  let thresholdMet = false

  function getMessageId(el: Element | null): number | null {
    const messageEl = el?.closest?.('[data-message-id]')
    if (!messageEl) return null
    const id = messageEl.getAttribute('data-message-id')
    return id != null ? Number(id) : null
  }

  function onMouseDown(event: MouseEvent) {
    if (event.button !== 0) return
    const target = event.target as HTMLElement
    if (!target.closest('[data-message-id]')) return
    const cId = chatId()
    if (cId == null) return

    startY = event.clientY
    active = true
    thresholdMet = false
  }

  function onMouseMove(event: MouseEvent) {
    if (!active) return

    if (!thresholdMet) {
      const dy = Math.abs(event.clientY - startY)
      if (dy < verticalThreshold) return
      thresholdMet = true

      const selectionStore = useSelectionStore()
      const cId = chatId()
      if (cId == null) {
        active = false
        return
      }

      if (!selectionStore.selectionMode) {
        selectionStore.enterSelectionMode(cId)
      }
      isDragSelecting.value = true
    }

    event.preventDefault()

    const el = document.elementFromPoint(event.clientX, event.clientY)
    const messageId = getMessageId(el as Element | null)
    if (messageId != null) {
      const selectionStore = useSelectionStore()
      selectionStore.addMessage(messageId)
    }
  }

  function onMouseUp() {
    if (!active) return
    active = false
    thresholdMet = false
    isDragSelecting.value = false
  }

  return {
    isDragSelecting,
    onMouseDown,
    onMouseMove,
    onMouseUp,
  }
}
