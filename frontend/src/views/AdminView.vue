<template>
  <div class="admin-body">
    <nav class="admin-nav">
      <div class="admin-nav-brand">BH Admin</div>
      <div class="admin-nav-links">
        <router-link to="/admin" :class="{ active: route.path === '/admin' }">Dashboard</router-link>
        <router-link to="/admin/users" :class="{ active: route.path.startsWith('/admin/users') }">Users</router-link>
        <router-link to="/admin/messages" :class="{ active: route.path === '/admin/messages' }">Messages</router-link>
        <router-link to="/admin/support" :class="{ active: route.path === '/admin/support' }">Support</router-link>
      </div>
      <div class="admin-nav-user">
        <span>{{ authStore.user?.username }}</span>
        &middot;
        <a href="#" @click.prevent="handleLogout">Logout</a>
      </div>
    </nav>
    <main class="admin-main">
      <router-view />
    </main>
  </div>
</template>

<script setup lang="ts">
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const route = useRoute()
const router = useRouter()
const authStore = useAuthStore()

function handleLogout() {
  authStore.logout()
  router.push('/login')
}
</script>
