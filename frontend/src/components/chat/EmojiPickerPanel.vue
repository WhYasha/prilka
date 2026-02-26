<template>
  <div class="emoji-picker-panel" :style="{ height: panelHeight + 'px' }">
    <div
      class="drag-handle"
      @pointerdown="onDragStart"
    >
      <div class="drag-handle-bar" />
    </div>
    <Picker
      :data="emojiIndex"
      :native="true"
      :show-preview="false"
      :show-skin-tones="false"
      :emoji-size="28"
      :per-line="8"
      :auto-focus="true"
      :infinite-scroll="true"
      :picker-styles="pickerStyles"
      :i18n="i18n"
      @select="onEmojiSelect"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { Picker, EmojiIndex } from 'emoji-mart-vue-fast/src'
import data from 'emoji-mart-vue-fast/data/all.json'

const emojiIndex = new EmojiIndex(data)

const emitEvent = defineEmits<{
  select: [emoji: string]
}>()

const i18n = {
  search: 'Поиск',
  notfound: 'Эмодзи не найдены',
  categories: {
    search: 'Результаты поиска',
    recent: 'Недавние',
    people: 'Смайлы и люди',
    nature: 'Животные и природа',
    foods: 'Еда и напитки',
    activity: 'Активность',
    places: 'Путешествия',
    objects: 'Объекты',
    symbols: 'Символы',
    flags: 'Флаги',
  },
}

const pickerStyles = {
  width: '100%',
  border: 'none',
  borderRadius: '0',
}

// Resize logic
const MIN_HEIGHT = 200
const MAX_HEIGHT = 500
const DEFAULT_HEIGHT = 320

const panelHeight = ref(DEFAULT_HEIGHT)
const isResizing = ref(false)
const startY = ref(0)
const startHeight = ref(0)

function onDragStart(e: PointerEvent) {
  isResizing.value = true
  startY.value = e.clientY
  startHeight.value = panelHeight.value
  ;(e.target as HTMLElement).setPointerCapture(e.pointerId)
  document.addEventListener('pointermove', onDragMove)
  document.addEventListener('pointerup', onDragEnd)
}

function onDragMove(e: PointerEvent) {
  if (!isResizing.value) return
  const delta = startY.value - e.clientY
  panelHeight.value = Math.min(MAX_HEIGHT, Math.max(MIN_HEIGHT, startHeight.value + delta))
}

function onDragEnd() {
  isResizing.value = false
  document.removeEventListener('pointermove', onDragMove)
  document.removeEventListener('pointerup', onDragEnd)
}

function onEmojiSelect(emoji: { native: string }) {
  emitEvent('select', emoji.native)
}

function onKeyDown(e: KeyboardEvent) {
  if (e.key === 'Escape') {
    emitEvent('select', '')
  }
}

onMounted(() => {
  document.addEventListener('keydown', onKeyDown)
})

onUnmounted(() => {
  document.removeEventListener('keydown', onKeyDown)
  document.removeEventListener('pointermove', onDragMove)
  document.removeEventListener('pointerup', onDragEnd)
})
</script>

<style scoped>
.emoji-picker-panel {
  width: 100%;
  background: var(--composer-bg);
  border-top: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  overflow: hidden;
  position: relative;
}

.drag-handle {
  width: 100%;
  height: 12px;
  cursor: ns-resize;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  user-select: none;
  touch-action: none;
}

.drag-handle-bar {
  width: 40px;
  height: 4px;
  border-radius: 2px;
  background: var(--text-muted);
  opacity: 0.4;
}

.drag-handle:hover .drag-handle-bar {
  opacity: 0.7;
}

.emoji-picker-panel :deep(.emoji-mart) {
  flex: 1;
  height: 100% !important;
  background: var(--composer-bg);
  color: var(--text-primary, #fff);
  font-family: inherit;
}

.emoji-picker-panel :deep(.emoji-mart-bar) {
  border-color: var(--border);
}

.emoji-picker-panel :deep(.emoji-mart-search input) {
  background: var(--input-bg);
  color: var(--text-primary, #fff);
  border-color: var(--border);
  border-radius: 8px;
}

.emoji-picker-panel :deep(.emoji-mart-category-label span) {
  background: var(--composer-bg);
  color: var(--text-muted);
}

.emoji-picker-panel :deep(.emoji-mart-scroll) {
  overflow-y: auto;
}

.emoji-picker-panel :deep(.emoji-mart-anchor) {
  color: var(--text-muted);
}

.emoji-picker-panel :deep(.emoji-mart-anchor-selected) {
  color: var(--accent);
}

.emoji-picker-panel :deep(.emoji-mart-anchor-bar) {
  background-color: var(--accent) !important;
}
</style>
