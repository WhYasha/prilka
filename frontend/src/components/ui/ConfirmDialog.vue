<template>
  <Teleport to="body">
    <div v-if="visible" class="modal-backdrop" @click.self="cancel" @keydown.esc="cancel">
      <div class="modal confirm-dialog" role="alertdialog" aria-modal="true">
        <div class="modal-header">
          <span class="modal-title">{{ options.title || 'Confirm' }}</span>
        </div>
        <div class="modal-body confirm-body">
          <p>{{ options.message }}</p>
        </div>
        <div class="confirm-actions">
          <button class="btn btn-secondary" @click="cancel" ref="cancelBtn">
            {{ options.cancelLabel || 'Cancel' }}
          </button>
          <button
            class="btn"
            :class="options.danger ? 'btn-danger' : 'btn-primary'"
            @click="doConfirm"
            ref="confirmBtn"
          >
            {{ options.confirmLabel || 'Confirm' }}
          </button>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { watch, nextTick, ref as vRef } from 'vue'
import { useConfirm } from '@/composables/useConfirm'

const { visible, options, confirm, cancel } = useConfirm()
const confirmBtn = vRef<HTMLButtonElement | null>(null)

function doConfirm() {
  confirm()
}

// Auto-focus confirm button when dialog opens
watch(visible, async (v) => {
  if (v) {
    await nextTick()
    confirmBtn.value?.focus()
  }
})

// Handle Enter key on the dialog
function onKeydown(e: KeyboardEvent) {
  if (!visible.value) return
  if (e.key === 'Escape') {
    cancel()
  }
}

if (typeof window !== 'undefined') {
  window.addEventListener('keydown', onKeydown)
}
</script>
