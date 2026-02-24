import { ref } from 'vue'

export function useRecorder() {
  const isRecording = ref(false)
  const seconds = ref(0)
  let recorder: MediaRecorder | null = null
  let chunks: Blob[] = []
  let timerInterval: ReturnType<typeof setInterval> | null = null

  async function startRecording(): Promise<void> {
    if (!navigator.mediaDevices?.getUserMedia) {
      throw new Error('MediaDevices not supported')
    }

    const stream = await navigator.mediaDevices.getUserMedia({ audio: true })
    chunks = []
    seconds.value = 0
    recorder = new MediaRecorder(stream)

    recorder.ondataavailable = (e) => {
      if (e.data.size) chunks.push(e.data)
    }

    recorder.start()
    isRecording.value = true

    timerInterval = setInterval(() => {
      seconds.value++
    }, 1000)
  }

  function stopRecording(): Promise<File> {
    return new Promise((resolve, reject) => {
      if (!recorder || recorder.state !== 'recording') {
        reject(new Error('Not recording'))
        return
      }

      recorder.onstop = () => {
        if (recorder?.stream) {
          recorder.stream.getTracks().forEach((t) => t.stop())
        }
        clearTimer()
        isRecording.value = false

        const mimeType = recorder?.mimeType || 'audio/webm'
        const ext = mimeType.includes('ogg') ? 'ogg' : 'webm'
        const blob = new Blob(chunks, { type: mimeType })
        const file = new File([blob], `voice.${ext}`, { type: mimeType })
        resolve(file)
      }

      recorder.stop()
    })
  }

  function cancelRecording() {
    if (recorder && recorder.state === 'recording') {
      recorder.onstop = () => {
        if (recorder?.stream) {
          recorder.stream.getTracks().forEach((t) => t.stop())
        }
      }
      recorder.stop()
    }
    clearTimer()
    isRecording.value = false
    seconds.value = 0
  }

  function clearTimer() {
    if (timerInterval) {
      clearInterval(timerInterval)
      timerInterval = null
    }
  }

  function formatTime(secs: number): string {
    const m = Math.floor(secs / 60)
    const s = secs % 60
    return `${m}:${String(s).padStart(2, '0')}`
  }

  return {
    isRecording,
    seconds,
    startRecording,
    stopRecording,
    cancelRecording,
    formatTime,
  }
}
