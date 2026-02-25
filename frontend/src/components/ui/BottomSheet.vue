<template>
  <Teleport to="body">
    <Transition name="bottom-sheet">
      <div v-if="visible" class="bottom-sheet-backdrop" @click.self="close">
        <div class="bottom-sheet" @click.stop>
          <div class="bottom-sheet-handle" />
          <slot />
        </div>
      </div>
    </Transition>
  </Teleport>
</template>

<script setup lang="ts">
const props = defineProps<{
  visible: boolean
}>()

const emit = defineEmits<{
  close: []
}>()

function close() {
  emit('close')
}
</script>

<style scoped>
.bottom-sheet-backdrop {
  position: fixed;
  inset: 0;
  z-index: 1000;
  background: rgba(0, 0, 0, 0.5);
  display: flex;
  align-items: flex-end;
  justify-content: center;
}

.bottom-sheet {
  background: var(--surface);
  border-radius: 16px 16px 0 0;
  width: 100%;
  max-width: 480px;
  max-height: 80vh;
  overflow-y: auto;
  overflow-x: hidden;
  padding: 8px 16px calc(24px + env(safe-area-inset-bottom, 0px));
}

.bottom-sheet-handle {
  width: 36px;
  height: 4px;
  border-radius: 2px;
  background: var(--border);
  margin: 0 auto 12px;
}

/* Slide-up transition */
.bottom-sheet-enter-active,
.bottom-sheet-leave-active {
  transition: opacity 0.25s ease;
}

.bottom-sheet-enter-active .bottom-sheet,
.bottom-sheet-leave-active .bottom-sheet {
  transition: transform 0.25s ease;
}

.bottom-sheet-enter-from,
.bottom-sheet-leave-to {
  opacity: 0;
}

.bottom-sheet-enter-from .bottom-sheet,
.bottom-sheet-leave-to .bottom-sheet {
  transform: translateY(100%);
}
</style>
