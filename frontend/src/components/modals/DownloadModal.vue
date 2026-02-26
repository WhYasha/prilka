<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal" style="max-width: 420px">
      <div class="modal-header">
        <h2 class="modal-title">Download BeHappy</h2>
        <button class="icon-btn" aria-label="Close" @click="emit('close')"><X :size="20" /></button>
      </div>
      <div class="modal-body" style="display: flex; flex-direction: column; gap: 12px">
        <button
          v-if="primaryPlatform"
          class="btn btn-primary btn-full"
          @click="downloadPrimary"
        >
          Download for {{ primaryPlatform }}
        </button>
        <details>
          <summary style="cursor: pointer; color: var(--accent)">Other platforms</summary>
          <div style="display: flex; flex-direction: column; gap: 8px; margin-top: 8px">
            <a
              href="/downloads/windows/MessengerSetup.exe"
              class="btn btn-ghost btn-full"
              download
            >Windows installer (.exe)</a>
            <a
              href="/downloads/macos/Messenger.dmg"
              class="btn btn-ghost btn-full"
              download
            >macOS installer (.dmg)</a>
          </div>
        </details>
        <p style="font-size: 0.8rem; color: var(--text-muted); margin: 0">
          Files not available yet? Check back soon.
        </p>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { X } from 'lucide-vue-next'

const emit = defineEmits<{ close: [] }>()

const ua = navigator.userAgent.toLowerCase()
const isWindows = ua.includes('win')
const isMac = ua.includes('mac') && !ua.includes('iphone') && !ua.includes('ipad')

const primaryPlatform = computed(() => {
  if (isWindows) return 'Windows'
  if (isMac) return 'macOS'
  return null
})

function downloadPrimary() {
  if (isWindows) window.location.href = '/downloads/windows/MessengerSetup.exe'
  else if (isMac) window.location.href = '/downloads/macos/Messenger.dmg'
}
</script>
