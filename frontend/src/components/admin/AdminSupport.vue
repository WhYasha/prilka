<template>
  <div>
    <h1 class="admin-page-title">Support</h1>

    <!-- Send form -->
    <div class="admin-support-form">
      <h2>Send Support Message</h2>
      <label>Target User</label>
      <select v-model="targetUser">
        <option value="">-- Select user --</option>
        <option v-for="u in users" :key="u.id" :value="u.id">
          {{ u.username }} ({{ u.display_name }})
        </option>
      </select>
      <label>Or Chat ID (direct)</label>
      <input v-model="targetChatId" type="text" placeholder="Chat ID" />
      <label>Message</label>
      <textarea v-model="content" placeholder="Type support message..." />
      <br />
      <button class="btn btn-primary" @click="send">Send as bh_support</button>
    </div>

    <!-- Support chats -->
    <h3 class="section-title">Support Chats</h3>
    <template v-if="chats.length">
      <div class="admin-table-wrap">
        <table class="admin-table">
          <thead>
            <tr><th>Chat ID</th><th>Type</th><th>Name</th><th>Members</th></tr>
          </thead>
          <tbody>
            <tr v-for="c in chats" :key="c.id">
              <td>{{ c.id }}</td>
              <td><span class="badge" :class="'badge-' + c.type">{{ c.type }}</span></td>
              <td>{{ c.name || c.title || '-' }}</td>
              <td>{{ c.member_count }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </template>
    <p v-else>No support chats yet.</p>

    <!-- Recent messages -->
    <h3 class="section-title">Recent Support Messages</h3>
    <template v-if="messages.length">
      <div class="admin-table-wrap">
        <table class="admin-table">
          <thead>
            <tr><th>ID</th><th>Chat</th><th>Content</th><th>Sent</th></tr>
          </thead>
          <tbody>
            <tr v-for="m in messages" :key="m.id">
              <td>{{ m.id }}</td>
              <td>{{ m.chat_name || m.chat_id }} <span class="badge" :class="'badge-' + m.chat_type">{{ m.chat_type }}</span></td>
              <td class="msg-content">{{ m.content }}</td>
              <td>{{ fmtDate(m.created_at) }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </template>
    <p v-else>No support messages yet.</p>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import {
  getSupportUsers,
  getSupportChats,
  getSupportMessages,
  sendSupportMessage,
} from '@/api/admin'
import type { User } from '@/api/types'
import { useToast } from '@/composables/useToast'

const { showToast } = useToast()

const users = ref<User[]>([])
const chats = ref<Array<{ id: number; type: string; name: string; title: string; member_count: number }>>([])
const messages = ref<Array<{ id: number; chat_id: number; chat_name: string; chat_type: string; content: string; created_at: string }>>([])

const targetUser = ref('')
const targetChatId = ref('')
const content = ref('')

async function load() {
  const [u, c, m] = await Promise.all([
    getSupportUsers(),
    getSupportChats(),
    getSupportMessages(),
  ])
  users.value = u.users || []
  chats.value = (c.chats || []) as typeof chats.value
  messages.value = (m.messages || []) as typeof messages.value
}

async function send() {
  if (!content.value.trim()) return
  const payload: { content: string; target_user_id?: number; chat_id?: number } = {
    content: content.value.trim(),
  }
  if (targetUser.value) {
    payload.target_user_id = parseInt(targetUser.value)
  } else if (targetChatId.value.trim()) {
    payload.chat_id = parseInt(targetChatId.value.trim())
  } else {
    showToast('Select a target user or enter a chat ID.')
    return
  }
  await sendSupportMessage(payload)
  content.value = ''
  showToast('Support message sent.')
  await load()
}

function fmtDate(s?: string): string {
  if (!s) return '-'
  const d = new Date(s)
  if (isNaN(d.getTime())) return s
  return d.toLocaleString()
}

onMounted(load)
</script>
