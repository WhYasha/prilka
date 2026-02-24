<template>
  <div>
    <h1 class="admin-page-title">Messages</h1>

    <div class="admin-filter-bar">
      <input v-model="filterUserId" type="text" placeholder="User ID" style="width: 100px" />
      <input v-model="filterChatId" type="text" placeholder="Chat ID" style="width: 100px" />
      <input
        v-model="filterText"
        type="text"
        placeholder="Search text"
        @keydown.enter="applyFilter"
      />
      <button class="btn btn-primary" @click="applyFilter">Filter</button>
      <button class="btn btn-muted" @click="clearFilter">Clear</button>
    </div>

    <p v-if="loading">Loading...</p>
    <template v-else-if="data">
      <div class="admin-table-wrap">
        <table class="admin-table">
          <thead>
            <tr>
              <th>ID</th><th>Chat</th><th>Sender</th><th>Type</th><th>Content</th><th>Sent</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="m in data.messages" :key="m.id">
              <td>{{ m.id }}</td>
              <td>
                {{ m.chat_name || m.chat_id }}
                <span class="badge" :class="'badge-' + m.chat_type">{{ m.chat_type }}</span>
              </td>
              <td>
                <router-link :to="`/admin/users/${m.sender_id}`">{{ m.sender_username }}</router-link>
              </td>
              <td>{{ m.message_type }}</td>
              <td class="msg-content">{{ m.content }}</td>
              <td>{{ fmtDate(m.created_at) }}</td>
            </tr>
          </tbody>
        </table>
      </div>

      <div v-if="data.total_pages > 1" class="admin-pagination">
        <a v-if="page > 1" href="#" @click.prevent="goPage(page - 1)">&laquo; Prev</a>
        <template v-for="i in data.total_pages" :key="i">
          <span v-if="i === page" class="current">{{ i }}</span>
          <a
            v-else-if="Math.abs(i - page) < 3 || i === 1 || i === data.total_pages"
            href="#"
            @click.prevent="goPage(i)"
          >{{ i }}</a>
        </template>
        <a v-if="page < data.total_pages" href="#" @click.prevent="goPage(page + 1)">Next &raquo;</a>
      </div>
      <span class="admin-pagination info">{{ data.total }} total messages</span>
    </template>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { getMessages } from '@/api/admin'
import type { AdminMessageList } from '@/api/types'

const data = ref<AdminMessageList | null>(null)
const loading = ref(true)
const filterUserId = ref('')
const filterChatId = ref('')
const filterText = ref('')
const page = ref(1)

async function load() {
  loading.value = true
  try {
    data.value = await getMessages({
      page: page.value,
      per_page: 50,
      user_id: filterUserId.value || undefined,
      chat_id: filterChatId.value || undefined,
      q: filterText.value || undefined,
    })
  } finally {
    loading.value = false
  }
}

function applyFilter() {
  page.value = 1
  load()
}

function clearFilter() {
  filterUserId.value = ''
  filterChatId.value = ''
  filterText.value = ''
  page.value = 1
  load()
}

function goPage(p: number) {
  page.value = p
  load()
}

function fmtDate(s?: string): string {
  if (!s) return '-'
  const d = new Date(s)
  if (isNaN(d.getTime())) return s
  return d.toLocaleString()
}

onMounted(load)
</script>
