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
  let initialMessageId: number | null = null

  function getMessageId(el: Element | null): number | null {
    const messageEl = el?.closest?.('[data-message-id]')
    if (!messageEl) return null
    const id = messageEl.getAttribute('data-message-id')
    return id != null ? Number(id) : null
  }

  function getNearestMessageId(x: number, y: number): number | null {
    const el = document.elementFromPoint(x, y)
    const direct = getMessageId(el as Element | null)
    if (direct != null) return direct

    // Fallback: find the nearest [data-message-id] element by vertical distance
    const messageEls = document.querySelectorAll('[data-message-id]')
    let closest: Element | null = null
    let minDist = Infinity
    for (const msgEl of messageEls) {
      const rect = msgEl.getBoundingClientRect()
      const centerY = rect.top + rect.height / 2
      const dist = Math.abs(y - centerY)
      if (dist < minDist) {
        minDist = dist
        closest = msgEl
      }
    }
    if (closest) {
      const id = closest.getAttribute('data-message-id')
      return id != null ? Number(id) : null
    }
    return null
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
    initialMessageId = getMessageId(target)
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
        selectionStore.enterSelectionMode(cId, initialMessageId ?? undefined)
      } else if (initialMessageId != null) {
        selectionStore.addMessage(initialMessageId)
      }
      isDragSelecting.value = true
    }

    event.preventDefault()

    const messageId = getNearestMessageId(event.clientX, event.clientY)
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
