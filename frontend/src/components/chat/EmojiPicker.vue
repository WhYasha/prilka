<template>
  <div
    v-if="visible"
    class="emoji-picker"
    :style="{ left: x + 'px', top: y + 'px' }"
    @click.stop
  >
    <button
      v-for="e in emojis"
      :key="e"
      class="emoji-pick-btn"
      @click="emit('select', e)"
    >
      {{ e }}
    </button>
  </div>
</template>

<script setup lang="ts">
defineProps<{
  visible: boolean
  x: number
  y: number
}>()

const emit = defineEmits<{
  select: [emoji: string]
  close: []
}>()

const emojis = [
  '\u{1F44D}', '\u{2764}\u{FE0F}', '\u{1F602}', '\u{1F62E}', '\u{1F622}', '\u{1F64F}',
  '\u{1F525}', '\u{1F389}', '\u{1F44F}', '\u{1F914}', '\u{1F60D}', '\u{1F621}',
  '\u{1F44E}', '\u{1F440}', '\u{1F680}', '\u{2705}', '\u{274C}', '\u{1F4AF}',
  '\u{1F31F}', '\u{1F60A}', '\u{1F92F}', '\u{1F973}', '\u{1F972}', '\u{1F917}',
]
</script>
