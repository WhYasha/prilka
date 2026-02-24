import { ref, type Ref } from 'vue'

export interface UseLongPressOptions {
  delay?: number
  scrollThreshold?: number
  onLongPress: (event: PointerEvent) => void
  onCancel?: () => void
}

export function useLongPress(options: UseLongPressOptions) {
  const {
    delay = 500,
    scrollThreshold = 5,
    onLongPress,
    onCancel,
  } = options

  const isPressed: Ref<boolean> = ref(false)
  let timer: ReturnType<typeof setTimeout> | null = null
  let startX = 0
  let startY = 0
  let fired = false

  function clear() {
    if (timer) {
      clearTimeout(timer)
      timer = null
    }
  }

  function onPointerDown(event: PointerEvent) {
    startX = event.clientX
    startY = event.clientY
    fired = false
    isPressed.value = true

    clear()
    timer = setTimeout(() => {
      fired = true
      onLongPress(event)
    }, delay)
  }

  function onPointerMove(event: PointerEvent) {
    if (!timer) return

    const dx = event.clientX - startX
    const dy = event.clientY - startY
    if (Math.abs(dx) > scrollThreshold || Math.abs(dy) > scrollThreshold) {
      cancel()
    }
  }

  function onPointerUp() {
    clear()
    isPressed.value = false
  }

  function cancel() {
    clear()
    isPressed.value = false
    if (!fired) {
      onCancel?.()
    }
  }

  return {
    isPressed,
    onPointerDown,
    onPointerUp,
    onPointerMove,
  }
}
