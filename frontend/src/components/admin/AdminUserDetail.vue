<template>
  <div>
    <p v-if="loading">Loading...</p>
    <template v-else-if="data?.user">
      <h1 class="admin-page-title">
        User #{{ data.user.id }} - {{ data.user.username }}
      </h1>

      <!-- Profile card -->
      <div class="admin-detail-card">
        <h2>Profile</h2>
        <dl class="detail-grid">
          <dt>ID</dt><dd>{{ data.user.id }}</dd>
          <dt>Username</dt><dd>{{ data.user.username }}</dd>
          <dt>Display Name</dt><dd>{{ data.user.display_name }}</dd>
          <dt>Email</dt><dd>{{ data.user.email }}</dd>
          <dt>Bio</dt><dd>{{ data.user.bio || '-' }}</dd>
          <dt>Status</dt>
          <dd>
            <span v-if="data.user.is_blocked" class="badge badge-blocked">Blocked</span>
            <span v-else-if="!data.user.is_active" class="badge badge-inactive">Inactive</span>
            <span v-else class="badge badge-active">Active</span>
            <span v-if="data.user.is_admin" class="badge badge-admin">Admin</span>
          </dd>
          <dt>Created</dt><dd>{{ fmtDate(data.user.created_at) }}</dd>
          <dt>Updated</dt><dd>{{ fmtDate(data.user.updated_at) }}</dd>
          <dt>Last Active</dt><dd>{{ fmtDate(data.user.last_activity) }}</dd>
          <dt>Messages</dt><dd>{{ data.message_count }}</dd>
        </dl>
      </div>

      <!-- Actions -->
      <div class="admin-actions">
        <button
          v-if="data.user.is_blocked"
          class="btn btn-success btn-sm"
          @click="doAction('unblock')"
        >Unblock</button>
        <button v-else class="btn btn-danger btn-sm" @click="doAction('block')">Block</button>
        <button class="btn btn-danger btn-sm" @click="doAction('soft-delete')">Soft Delete</button>
        <button class="btn btn-warning btn-sm" @click="doAction('toggle-admin')">
          {{ data.user.is_admin ? 'Remove Admin' : 'Make Admin' }}
        </button>
        <router-link class="btn btn-primary btn-sm" :to="`/admin/messages?user_id=${data.user.id}`">
          View Messages
        </router-link>
      </div>

      <!-- Support message -->
      <div class="admin-support-form">
        <h2>Send Support Message</h2>
        <label>Message</label>
        <textarea v-model="supportMsg" placeholder="Type support message..." />
        <br />
        <button class="btn btn-primary" @click="sendSupport">Send as bh_support</button>
      </div>

      <!-- Chats -->
      <h3 class="section-title">Chats ({{ data.chats?.length || 0 }})</h3>
      <div v-if="data.chats?.length" class="admin-table-wrap">
        <table class="admin-table">
          <thead>
            <tr><th>Chat ID</th><th>Type</th><th>Name</th><th>Role</th><th>Members</th><th>Joined</th></tr>
          </thead>
          <tbody>
            <tr v-for="c in data.chats" :key="c.id">
              <td>{{ c.id }}</td>
              <td><span class="badge" :class="'badge-' + c.type">{{ c.type }}</span></td>
              <td>{{ c.name || c.title || '-' }}</td>
              <td>{{ c.role }}</td>
              <td>{{ c.member_count }}</td>
              <td>{{ fmtDate(c.joined_at) }}</td>
            </tr>
          </tbody>
        </table>
      </div>

      <!-- Messages -->
      <h3 class="section-title">Recent Messages ({{ data.messages?.length || 0 }})</h3>
      <div v-if="data.messages?.length" class="admin-table-wrap">
        <table class="admin-table">
          <thead>
            <tr><th>ID</th><th>Chat</th><th>Type</th><th>Content</th><th>Sent</th></tr>
          </thead>
          <tbody>
            <tr v-for="m in data.messages" :key="m.id">
              <td>{{ m.id }}</td>
              <td>{{ m.chat_name || m.chat_id }} <span class="badge" :class="'badge-' + m.chat_type">{{ m.chat_type }}</span></td>
              <td>{{ m.message_type }}</td>
              <td class="msg-content">{{ m.content }}</td>
              <td>{{ fmtDate(m.created_at) }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </template>
    <p v-else>User not found.</p>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { getUserDetail, userAction, sendSupportMessage } from '@/api/admin'
import type { AdminUserDetail } from '@/api/types'

const route = useRoute()
const data = ref<AdminUserDetail | null>(null)
const loading = ref(true)
const supportMsg = ref('')

const userId = parseInt(route.params.id as string)

async function load() {
  loading.value = true
  try {
    data.value = await getUserDetail(userId)
  } finally {
    loading.value = false
  }
}

async function doAction(action: 'block' | 'unblock' | 'soft-delete' | 'toggle-admin') {
  const msg = action === 'soft-delete'
    ? 'Are you sure you want to soft-delete this user?'
    : `Are you sure you want to ${action} this user?`
  if (!confirm(msg)) return
  await userAction(userId, action)
  await load()
}

async function sendSupport() {
  if (!supportMsg.value.trim()) return
  await sendSupportMessage({
    target_user_id: userId,
    content: supportMsg.value.trim(),
  })
  supportMsg.value = ''
  alert('Support message sent.')
}

function fmtDate(s?: string): string {
  if (!s) return '-'
  const d = new Date(s)
  if (isNaN(d.getTime())) return s
  return d.toLocaleString()
}

onMounted(load)
</script>
