<template>
  <Transition name="slide-down">
    <div v-if="selectionStore.selectionMode" class="selection-bar">
      <button class="selection-bar-btn cancel-btn" @click="selectionStore.exitSelectionMode()">
        Cancel
      </button>
      <span class="selection-bar-count">{{ selectionStore.selectedCount }} selected</span>
      <div class="selection-bar-actions">
        <button class="selection-bar-btn forward-btn" :disabled="selectionStore.selectedCount === 0" @click="emit('forward')">
          Forward ({{ selectionStore.selectedCount }})
        </button>
        <button class="selection-bar-btn delete-btn" :disabled="selectionStore.selectedCount === 0" @click="emit('delete')">
          Delete ({{ selectionStore.selectedCount }})
        </button>
      </div>
    </div>
  </Transition>
</template>

<script setup lang="ts">
import { useSelectionStore } from '@/stores/selection'

const emit = defineEmits<{
  forward: []
  delete: []
}>()

const selectionStore = useSelectionStore()
</script>

<style scoped>
.selection-bar {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 8px 16px;
  background: var(--color-bg-secondary, #1e1e1e);
  border-bottom: 1px solid var(--color-border, #333);
  min-height: 48px;
}

.selection-bar-count {
  flex: 1;
  font-size: 0.95rem;
  font-weight: 500;
  color: var(--color-text-primary, #fff);
}

.selection-bar-actions {
  display: flex;
  gap: 8px;
}

.selection-bar-btn {
  padding: 6px 16px;
  border: none;
  border-radius: 8px;
  font-size: 0.9rem;
  font-weight: 500;
  cursor: pointer;
  background: transparent;
  color: var(--color-accent, #5b9bd5);
  transition: opacity 0.15s;
}

.selection-bar-btn:disabled {
  opacity: 0.4;
  cursor: default;
}

.cancel-btn {
  color: var(--color-text-secondary, #aaa);
}

.delete-btn {
  color: var(--color-danger, #e53935);
}

.slide-down-enter-active,
.slide-down-leave-active {
  transition: transform 0.2s ease, opacity 0.2s ease;
}

.slide-down-enter-from,
.slide-down-leave-to {
  transform: translateY(-100%);
  opacity: 0;
}
</style>
