<template>
  <div class="sticker-grid">
    <button
      v-for="s in stickerList"
      :key="s.id"
      class="sticker-btn"
      :title="s.label"
      @click="emit('select', s.id)"
    >
      <img :src="s.url" :alt="s.label" />
    </button>
    <div v-if="stickerList.length === 0" class="empty-state-small" style="color: var(--text-muted)">
      No stickers available
    </div>
  </div>
</template>

<script setup lang="ts">
import { inject, ref, computed, type Ref } from 'vue'
import type { Sticker } from '@/api/types'

const emit = defineEmits<{
  select: [stickerId: number]
}>()

const stickersRef = inject<Ref<Sticker[]>>('stickers', ref([]))
const stickerList = computed(() => stickersRef.value)
</script>
