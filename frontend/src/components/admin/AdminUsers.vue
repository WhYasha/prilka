<template>
  <div>
    <h1 class="admin-page-title">Users</h1>

    <!-- Filter bar -->
    <div class="admin-filter-bar">
      <input
        v-model="filterQ"
        type="text"
        placeholder="Search username / display name"
        @keydown.enter="applyFilter"
      />
      <select v-model="filterStatus">
        <option value="">All</option>
        <option value="active">Active</option>
        <option value="blocked">Blocked</option>
        <option value="admin">Admin</option>
        <option value="inactive">Inactive</option>
      </select>
      <button class="btn btn-primary" @click="applyFilter">Filter</button>
      <button class="btn btn-muted" @click="clearFilter">Clear</button>
    </div>

    <p v-if="loading">Loading...</p>
    <template v-else-if="data">
      <div class="admin-table-wrap">
        <table class="admin-table">
          <thead>
            <tr>
              <th>ID</th>
              <th>Username</th>
              <th>Display Name</th>
              <th>Status</th>
              <th>Created</th>
              <th>Last Active</th>
              <th />
            </tr>
          </thead>
          <tbody>
            <tr v-for="u in data.users" :key="u.id">
              <td>{{ u.id }}</td>
              <td>{{ u.username }}</td>
              <td>{{ u.display_name }}</td>
              <td>
                <span v-if="u.is_blocked" class="badge badge-blocked">Blocked</span>
                <span v-else-if="!u.is_active" class="badge badge-inactive">Inactive</span>
                <span v-else class="badge badge-active">Active</span>
                <span v-if="u.is_admin" class="badge badge-admin">Admin</span>
              </td>
              <td>{{ fmtDate(u.created_at) }}</td>
              <td>{{ fmtDate(u.last_activity) }}</td>
              <td><router-link :to="`/admin/users/${u.id}`">View</router-link></td>
            </tr>
          </tbody>
        </table>
      </div>

      <!-- Pagination -->
      <div v-if="data.total_pages > 1" class="admin-pagination">
        <a v-if="page > 1" href="#" @click.prevent="goPage(page - 1)">&laquo; Prev</a>
        <template v-for="i in data.total_pages" :key="i">
          <span v-if="i === page" class="current">{{ i }}</span>
          <a
            v-else-if="Math.abs(i - page) < 3 || i === 1 || i === data.total_pages"
            href="#"
            @click.prevent="goPage(i)"
          >{{ i }}</a>
          <span v-else-if="Math.abs(i - page) === 3">...</span>
        </template>
        <a v-if="page < data.total_pages" href="#" @click.prevent="goPage(page + 1)">Next &raquo;</a>
      </div>
      <span class="admin-pagination info">{{ data.total }} total users</span>
    </template>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { getUsers } from '@/api/admin'
import type { AdminUserList } from '@/api/types'

const data = ref<AdminUserList | null>(null)
const loading = ref(true)
const filterQ = ref('')
const filterStatus = ref('')
const page = ref(1)

async function load() {
  loading.value = true
  try {
    data.value = await getUsers({
      page: page.value,
      per_page: 20,
      q: filterQ.value || undefined,
      status: filterStatus.value || undefined,
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
  filterQ.value = ''
  filterStatus.value = ''
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
