import { onMounted, onUnmounted, ref, type Ref } from 'vue'

export function useInfiniteScroll(
  containerRef: Ref<HTMLElement | null>,
  onLoadMore: () => Promise<unknown[]>,
  options?: { threshold?: number },
) {
  const loading = ref(false)
  const hasMore = ref(true)
  const threshold = options?.threshold ?? 50

  async function handleScroll() {
    if (!containerRef.value || loading.value || !hasMore.value) return
    const el = containerRef.value
    if (el.scrollTop <= threshold) {
      loading.value = true
      const prevHeight = el.scrollHeight
      try {
        const items = await onLoadMore()
        if (!items || items.length === 0) {
          hasMore.value = false
        } else {
          // Maintain scroll position
          const newHeight = el.scrollHeight
          el.scrollTop += newHeight - prevHeight
        }
      } finally {
        loading.value = false
      }
    }
  }

  onMounted(() => {
    containerRef.value?.addEventListener('scroll', handleScroll, { passive: true })
  })

  onUnmounted(() => {
    containerRef.value?.removeEventListener('scroll', handleScroll)
  })

  return { loading, hasMore }
}
