<template>
  <div class="search-bar">
    <div class="search-bar-header">
      <button class="icon-btn" aria-label="Close search" @click="emit('close')">&#8592;</button>
      <input
        ref="inputRef"
        v-model="query"
        class="search-bar-input"
        type="text"
        placeholder="Search messages..."
        @keydown.escape="emit('close')"
        @contextmenu="onContextMenu"
      />
      <button v-if="query" class="icon-btn" aria-label="Clear" @click="query = ''">&#10005;</button>
    </div>
    <div v-if="loading && results.length === 0" class="search-loading">Searching...</div>
    <div v-else-if="searched && results.length === 0 && !loading" class="search-empty">No messages found</div>
    <div v-else-if="results.length > 0" ref="resultsRef" class="search-results" @scroll="onResultsScroll">
      <div
        v-for="msg in results"
        :key="msg.id"
        class="search-result-item"
        @click="emit('jumpToMessage', msg.id)"
      >
        <div class="search-result-sender">{{ msg.sender_display_name || msg.sender_username || 'Unknown' }}</div>
        <div class="search-result-snippet" v-html="highlightMatch(msg.content || '', query)"></div>
        <div class="search-result-date">{{ formatDate(msg.created_at) }}</div>
      </div>
      <div v-if="loading" class="search-loading">Loading more...</div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, watch, onMounted, nextTick } from 'vue'
import { searchMessages } from '@/api/messages'
import type { Message } from '@/api/types'

const props = defineProps<{ chatId: number }>()
const emit = defineEmits<{
  close: []
  jumpToMessage: [messageId: number]
}>()

const inputRef = ref<HTMLInputElement | null>(null)
const resultsRef = ref<HTMLElement | null>(null)
const query = ref('')
const results = ref<Message[]>([])
const loading = ref(false)
const searched = ref(false)
const hasMore = ref(true)

let debounceTimer: ReturnType<typeof setTimeout> | null = null

function onContextMenu(event: MouseEvent) {
  event.preventDefault()
  event.stopPropagation()
  const el = inputRef.value
  if (!el) return
  if (el.selectionStart !== el.selectionEnd) {
    window.dispatchEvent(new CustomEvent('show-text-format-menu', {
      detail: { x: event.clientX, y: event.clientY, target: el }
    }))
  }
}

onMounted(() => {
  nextTick(() => inputRef.value?.focus())
})

watch(query, (val) => {
  if (debounceTimer) clearTimeout(debounceTimer)
  if (!val.trim()) {
    results.value = []
    searched.value = false
    hasMore.value = true
    return
  }
  debounceTimer = setTimeout(() => doSearch(val.trim()), 300)
})

async function doSearch(q: string) {
  loading.value = true
  searched.value = true
  hasMore.value = true
  try {
    const data = await searchMessages(props.chatId, q, { limit: 20 })
    results.value = data
    if (data.length < 20) hasMore.value = false
  } catch {
    results.value = []
  } finally {
    loading.value = false
  }
}

async function loadMore() {
  if (loading.value || !hasMore.value || results.value.length === 0) return
  const minId = Math.min(...results.value.map((m) => m.id))
  loading.value = true
  try {
    const data = await searchMessages(props.chatId, query.value.trim(), {
      limit: 20,
      before_id: minId,
    })
    if (data.length < 20) hasMore.value = false
    results.value = [...results.value, ...data]
  } catch {
    // ignore
  } finally {
    loading.value = false
  }
}

function onResultsScroll() {
  if (!resultsRef.value) return
  const el = resultsRef.value
  if (el.scrollTop + el.clientHeight >= el.scrollHeight - 40) {
    loadMore()
  }
}

function highlightMatch(text: string, q: string): string {
  if (!q) return escapeHtml(text)
  const escaped = escapeHtml(text)
  const escapedQ = escapeHtml(q)
  const re = new RegExp(escapedQ.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'), 'gi')
  return escaped.replace(re, '<mark>$&</mark>')
}

function escapeHtml(str: string): string {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;')
}

function formatDate(isoStr: string): string {
  if (!isoStr) return ''
  const d = new Date(isoStr.endsWith('Z') ? isoStr : isoStr + 'Z')
  const now = new Date()
  if (d.toDateString() === now.toDateString()) {
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
  }
  return d.toLocaleDateString([], { month: 'short', day: 'numeric' }) +
    ' ' + d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
}
</script>
