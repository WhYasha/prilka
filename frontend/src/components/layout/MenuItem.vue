<template>
  <button
    class="menu-item"
    :class="{ 'menu-item--danger': danger, 'menu-item--disabled': disabled }"
    :disabled="disabled"
    @click="emit('click')"
  >
    <component
      v-if="icon"
      :is="icon"
      class="menu-item__icon"
      :size="24"
      :stroke-width="1.8"
    />
    <span class="menu-item__label">{{ label }}</span>
    <span v-if="badge" class="menu-item__badge">{{ badge }}</span>
    <slot name="right" />
    <component
      v-if="chevron"
      :is="ChevronRight"
      class="menu-item__chevron"
      :size="18"
      :stroke-width="1.8"
    />
  </button>
</template>

<script setup lang="ts">
import { ChevronRight } from 'lucide-vue-next'
import type { Component } from 'vue'

defineProps<{
  icon?: Component
  label: string
  badge?: string | number
  chevron?: boolean
  danger?: boolean
  disabled?: boolean
}>()

const emit = defineEmits<{
  click: []
}>()
</script>

<style scoped>
.menu-item {
  display: flex;
  align-items: center;
  gap: var(--menu-item-gap);
  width: 100%;
  height: var(--menu-item-height);
  padding: 0 var(--menu-item-px);
  background: none;
  border: none;
  border-radius: 0;
  color: var(--menu-text-color);
  font-size: .9375rem;
  text-align: left;
  cursor: pointer;
  transition: background var(--transition-fast);
  user-select: none;
}

.menu-item:hover {
  background: var(--menu-hover-bg);
}

.menu-item:active {
  background: var(--sidebar-active);
}

.menu-item--danger {
  color: var(--danger);
}

.menu-item--danger .menu-item__icon {
  color: var(--danger);
}

.menu-item--disabled {
  opacity: .45;
  cursor: not-allowed;
}

.menu-item--disabled:hover {
  background: none;
}

.menu-item__icon {
  flex-shrink: 0;
  width: var(--menu-item-icon-size);
  height: var(--menu-item-icon-size);
  color: var(--menu-icon-color);
}

.menu-item__label {
  flex: 1;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.menu-item__badge {
  flex-shrink: 0;
  min-width: 20px;
  height: 20px;
  padding: 0 6px;
  border-radius: 10px;
  background: var(--accent);
  color: #fff;
  font-size: .75rem;
  font-weight: 600;
  line-height: 20px;
  text-align: center;
}

.menu-item__chevron {
  flex-shrink: 0;
  color: var(--text-muted);
}
</style>
