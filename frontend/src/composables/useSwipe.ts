import { ref, type Ref } from 'vue'

export interface SwipeOptions {
  threshold?: number
  direction?: 'left' | 'right' | 'both'
}

export interface SwipeReturn {
  swipeOffset: Ref<number>
  isSwiping: Ref<boolean>
  onTouchStart: (e: TouchEvent) => void
  onTouchMove: (e: TouchEvent) => void
  onTouchEnd: () => void
}

export function useSwipe(options?: SwipeOptions): SwipeReturn {
  const threshold = options?.threshold ?? 80
  const direction = options?.direction ?? 'both'

  const swipeOffset = ref(0)
  const isSwiping = ref(false)

  let startX = 0
  let startY = 0
  let tracking = false
  let directionLocked = false
  let isHorizontal = false
  let animationFrame: number | null = null

  function onTouchStart(e: TouchEvent) {
    const touch = e.touches[0]!
    startX = touch.clientX
    startY = touch.clientY
    tracking = true
    directionLocked = false
    isHorizontal = false
  }

  function onTouchMove(e: TouchEvent) {
    if (!tracking) return

    const touch = e.touches[0]!
    const dx = touch.clientX - startX
    const dy = touch.clientY - startY

    if (!directionLocked) {
      if (Math.abs(dx) < 5 && Math.abs(dy) < 5) return
      directionLocked = true
      isHorizontal = Math.abs(dx) > Math.abs(dy)
      if (!isHorizontal) {
        tracking = false
        return
      }
      isSwiping.value = true
    }

    let offset = dx

    if (direction === 'left' && offset > 0) offset = 0
    if (direction === 'right' && offset < 0) offset = 0

    swipeOffset.value = offset
  }

  function onTouchEnd() {
    if (!tracking && !isSwiping.value) return
    tracking = false

    const current = swipeOffset.value
    const absOffset = Math.abs(current)

    if (absOffset >= threshold) {
      snapTo(current > 0 ? threshold : -threshold)
    } else {
      springBack()
    }

    isSwiping.value = false
  }

  function snapTo(target: number) {
    animateTo(target)
  }

  function springBack() {
    animateTo(0)
  }

  function animateTo(target: number) {
    if (animationFrame !== null) {
      cancelAnimationFrame(animationFrame)
    }

    const start = swipeOffset.value
    const distance = target - start
    const duration = 250
    const startTime = performance.now()

    function step(now: number) {
      const elapsed = now - startTime
      const progress = Math.min(elapsed / duration, 1)
      const eased = 1 - Math.pow(1 - progress, 3)

      swipeOffset.value = start + distance * eased

      if (progress < 1) {
        animationFrame = requestAnimationFrame(step)
      } else {
        swipeOffset.value = target
        animationFrame = null
      }
    }

    animationFrame = requestAnimationFrame(step)
  }

  return {
    swipeOffset,
    isSwiping,
    onTouchStart,
    onTouchMove,
    onTouchEnd,
  }
}
