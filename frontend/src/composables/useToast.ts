import { ref } from 'vue'

const message = ref('')
const visible = ref(false)
let timer: ReturnType<typeof setTimeout> | null = null

export function useToast() {
  function showToast(msg: string, duration = 3000) {
    message.value = msg
    visible.value = true
    if (timer) clearTimeout(timer)
    timer = setTimeout(() => {
      visible.value = false
    }, duration)
  }

  return {
    message,
    visible,
    showToast,
  }
}
