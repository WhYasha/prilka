<template>
  <Teleport to="body">
    <div
      v-if="visible"
      class="text-format-menu"
      :style="{ left: posX + 'px', top: posY + 'px' }"
      @click.stop
      @contextmenu.prevent
    >
      <button class="ctx-item" @click="applyFormat('**')">
        <strong>B</strong>&ensp;Bold
      </button>
      <button class="ctx-item" @click="applyFormat('*')">
        <em>I</em>&ensp;Italic
      </button>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'

const visible = ref(false)
const posX = ref(0)
const posY = ref(0)
const targetInput = ref<HTMLTextAreaElement | HTMLInputElement | null>(null)

function onShowTextFormatMenu(e: CustomEvent) {
  targetInput.value = e.detail.target
  posX.value = e.detail.x
  posY.value = e.detail.y
  visible.value = true

  requestAnimationFrame(() => {
    const el = document.querySelector('.text-format-menu') as HTMLElement | null
    if (!el) return
    const rect = el.getBoundingClientRect()
    if (rect.right > window.innerWidth) {
      posX.value = window.innerWidth - rect.width - 8
    }
    if (rect.bottom > window.innerHeight) {
      posY.value = window.innerHeight - rect.height - 8
    }
  })
}

function hide() {
  visible.value = false
  targetInput.value = null
}

function onClickOutside() {
  if (visible.value) {
    hide()
  }
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === 'Escape' && visible.value) {
    hide()
  }
}

function applyFormat(marker: string) {
  const el = targetInput.value
  if (!el) return

  const start = el.selectionStart
  const end = el.selectionEnd
  if (start === null || end === null || start === end) {
    hide()
    return
  }

  const selected = el.value.substring(start, end)
  const formatted = `${marker}${selected}${marker}`

  el.setRangeText(formatted, start, end, 'end')
  el.dispatchEvent(new Event('input', { bubbles: true }))
  el.focus()

  hide()
}

onMounted(() => {
  window.addEventListener('show-text-format-menu', onShowTextFormatMenu as EventListener)
  document.addEventListener('click', onClickOutside)
  document.addEventListener('keydown', onKeydown)
})

onUnmounted(() => {
  window.removeEventListener('show-text-format-menu', onShowTextFormatMenu as EventListener)
  document.removeEventListener('click', onClickOutside)
  document.removeEventListener('keydown', onKeydown)
})
</script>

<style scoped>
.text-format-menu {
  position: fixed;
  z-index: 9999;
  min-width: 140px;
  background: var(--ctx-menu-bg);
  border: 1px solid var(--ctx-menu-border);
  border-radius: 8px;
  box-shadow: var(--ctx-menu-shadow);
  padding: 4px 0;
  display: flex;
  flex-direction: column;
  animation: ctx-menu-in var(--transition-fast) ease;
}

.ctx-item {
  display: block;
  width: 100%;
  padding: 10px 16px;
  border: none;
  background: none;
  text-align: left;
  font-size: 14px;
  color: var(--ctx-menu-text);
  cursor: pointer;
  transition: background var(--transition-fast);
}

.ctx-item:hover {
  background: var(--ctx-menu-hover);
}
</style>
