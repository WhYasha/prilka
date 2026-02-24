<template>
  <div class="profile-actions">
    <button
      v-for="action in actions"
      :key="action.label"
      class="profile-action-btn"
      :class="{ 'profile-action-btn--danger': action.danger }"
      :disabled="action.disabled"
      @click="emit(action.event as any)"
    >
      <span class="profile-action-btn__circle">
        <component
          :is="action.icon"
          :size="22"
          :stroke-width="1.8"
        />
      </span>
      <span class="profile-action-btn__label">{{ action.label }}</span>
    </button>
  </div>
</template>

<script setup lang="ts">
import { computed, type Component } from 'vue'
import { MessageCircle, BellOff, MoreHorizontal } from 'lucide-vue-next'

const props = defineProps<{
  showMute?: boolean
  showMore?: boolean
  muted?: boolean
}>()

const emit = defineEmits<{
  message: []
  mute: []
  more: []
}>()

interface ActionItem {
  icon: Component
  label: string
  event: string
  danger?: boolean
  disabled?: boolean
}

const actions = computed<ActionItem[]>(() => {
  const items: ActionItem[] = [
    { icon: MessageCircle, label: 'Message', event: 'message' },
  ]

  if (props.showMute !== false) {
    items.push({
      icon: BellOff,
      label: props.muted ? 'Unmute' : 'Mute',
      event: 'mute',
    })
  }

  if (props.showMore !== false) {
    items.push({
      icon: MoreHorizontal,
      label: 'More',
      event: 'more',
    })
  }

  return items
})
</script>

<style scoped>
.profile-actions {
  display: flex;
  justify-content: center;
  gap: 32px;
  padding: 16px 0;
}

.profile-action-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  background: none;
  border: none;
  cursor: pointer;
  color: var(--accent);
  transition: opacity var(--transition-fast);
  user-select: none;
}

.profile-action-btn:hover {
  opacity: .8;
}

.profile-action-btn:disabled {
  opacity: .4;
  cursor: not-allowed;
}

.profile-action-btn--danger {
  color: var(--danger);
}

.profile-action-btn__circle {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 48px;
  height: 48px;
  border-radius: 50%;
  background: color-mix(in srgb, var(--accent) 12%, transparent);
  transition: background var(--transition-fast);
}

.profile-action-btn:hover .profile-action-btn__circle {
  background: color-mix(in srgb, var(--accent) 20%, transparent);
}

.profile-action-btn--danger .profile-action-btn__circle {
  background: color-mix(in srgb, var(--danger) 12%, transparent);
}

.profile-action-btn__label {
  font-size: .75rem;
  font-weight: 500;
  line-height: 1;
}
</style>
