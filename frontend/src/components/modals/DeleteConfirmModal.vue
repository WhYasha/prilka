<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <div class="modal modal-sm">
      <div class="modal-header">
        <h2 class="modal-title">Delete Message</h2>
        <button class="icon-btn" @click="emit('close')">&#10005;</button>
      </div>
      <div class="modal-body">
        <p class="delete-confirm-text">
          Are you sure you want to delete
          {{ messageCount > 1 ? `these ${messageCount} messages` : 'this message' }}?
        </p>
        <label v-if="canDeleteForEveryone" class="delete-checkbox-label">
          <input v-model="deleteForEveryone" type="checkbox" class="delete-checkbox" />
          Delete for everyone
        </label>
      </div>
      <div class="modal-actions">
        <button class="btn btn-ghost" @click="emit('close')">Cancel</button>
        <button class="btn btn-danger" @click="confirmDelete">Delete</button>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'

const props = withDefaults(defineProps<{
  messageCount?: number
  canDeleteForEveryone?: boolean
}>(), {
  messageCount: 1,
  canDeleteForEveryone: false,
})

const emit = defineEmits<{
  close: []
  confirm: [deleteForEveryone: boolean]
}>()

const deleteForEveryone = ref(false)

function confirmDelete() {
  emit('confirm', deleteForEveryone.value)
}
</script>

<style scoped>
.modal-sm {
  max-width: 400px;
}

.delete-confirm-text {
  margin: 0 0 1rem;
  color: var(--text-primary);
  font-size: 0.95rem;
}

.delete-checkbox-label {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  cursor: pointer;
  font-size: 0.9rem;
  color: var(--text-secondary);
}

.delete-checkbox {
  width: 1rem;
  height: 1rem;
  cursor: pointer;
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 0.5rem;
  padding: 1rem;
}

.btn-danger {
  background: var(--color-danger, #e53935);
  color: #fff;
  border: none;
  border-radius: 0.5rem;
  padding: 0.5rem 1.25rem;
  cursor: pointer;
  font-weight: 500;
}

.btn-danger:hover {
  background: var(--color-danger-hover, #c62828);
}
</style>
