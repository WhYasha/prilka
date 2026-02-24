<template>
  <div class="pin-bar" @click="$emit('click')">
    <span class="pin-bar-icon">&#128204;</span>
    <div class="pin-bar-content">
      <span class="pin-bar-sender">{{ senderName }}</span>
      <span class="pin-bar-text">{{ displayText }}</span>
    </div>
    <button class="pin-bar-close" @click.stop="$emit('dismiss')" title="Dismiss">&times;</button>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { Message } from '@/api/types'

const props = defineProps<{
  message: Message
}>()

defineEmits<{
  click: []
  dismiss: []
}>()

const senderName = computed(() => {
  return props.message.sender_display_name || props.message.sender_username || 'Unknown'
})

const displayText = computed(() => {
  const msg = props.message
  switch (msg.message_type) {
    case 'sticker':
      return msg.sticker_label || 'Sticker'
    case 'voice':
      return 'Voice message'
    case 'file':
      return 'File'
    default:
      return msg.content || ''
  }
})
</script>
