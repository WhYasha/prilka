<template>
  <div class="profile-info-list">
    <div
      v-for="field in fields"
      :key="field.label"
      class="profile-info-item"
      :class="{ 'profile-info-item--copyable': field.copyable }"
      @click="field.copyable ? copyValue(field.value) : undefined"
    >
      <component
        :is="field.icon"
        class="profile-info-item__icon"
        :size="20"
        :stroke-width="1.8"
      />
      <div class="profile-info-item__content">
        <span class="profile-info-item__value">{{ field.value }}</span>
        <span class="profile-info-item__label">{{ field.label }}</span>
      </div>
      <component
        v-if="field.copyable"
        :is="copiedField === field.label ? Check : Copy"
        class="profile-info-item__copy"
        :size="16"
        :stroke-width="1.8"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, ref, type Component } from 'vue'
import { AtSign, FileText, Hash, Copy, Check } from 'lucide-vue-next'

export interface InfoField {
  icon: Component
  label: string
  value: string
  copyable?: boolean
}

const props = defineProps<{
  username?: string
  bio?: string
  userId?: number | string
}>()

const copiedField = ref<string | null>(null)

const fields = computed<InfoField[]>(() => {
  const items: InfoField[] = []

  if (props.username) {
    items.push({
      icon: AtSign,
      label: 'Username',
      value: `@${props.username}`,
      copyable: true,
    })
  }

  if (props.bio) {
    items.push({
      icon: FileText,
      label: 'Bio',
      value: props.bio,
    })
  }

  if (props.userId != null) {
    items.push({
      icon: Hash,
      label: 'User ID',
      value: String(props.userId),
      copyable: true,
    })
  }

  return items
})

async function copyValue(value: string) {
  try {
    await navigator.clipboard.writeText(value.replace(/^@/, ''))
    const label = fields.value.find(f => f.value === value)?.label ?? null
    copiedField.value = label
    setTimeout(() => {
      copiedField.value = null
    }, 1500)
  } catch {
    // Clipboard API not available
  }
}
</script>

<style scoped>
.profile-info-list {
  display: flex;
  flex-direction: column;
  background: var(--profile-section-bg);
  border-radius: var(--radius);
  overflow: hidden;
}

.profile-info-item {
  display: flex;
  align-items: flex-start;
  gap: 14px;
  padding: 12px var(--menu-item-px);
  transition: background var(--transition-fast);
}

.profile-info-item + .profile-info-item {
  border-top: 1px solid var(--profile-divider);
}

.profile-info-item--copyable {
  cursor: pointer;
}

.profile-info-item--copyable:hover {
  background: var(--sidebar-hover);
}

.profile-info-item__icon {
  color: var(--text-muted);
  flex-shrink: 0;
  margin-top: 2px;
}

.profile-info-item__content {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
  flex: 1;
}

.profile-info-item__value {
  font-size: .9375rem;
  color: var(--text);
  line-height: 1.35;
  word-break: break-word;
}

.profile-info-item__label {
  font-size: .75rem;
  color: var(--text-muted);
  line-height: 1;
}

.profile-info-item__copy {
  color: var(--text-muted);
  flex-shrink: 0;
  margin-top: 2px;
  opacity: 0;
  transition: opacity var(--transition-fast);
}

.profile-info-item--copyable:hover .profile-info-item__copy {
  opacity: 1;
}
</style>
