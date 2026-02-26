<template>
  <div :class="['auth-page', { 'admin-login-page': isAdminHost }]">
    <div :class="['auth-card', { 'admin-login-card': isAdminHost }]">
      <div v-if="isAdminHost" class="admin-login-icon">
        <svg width="48" height="48" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
          <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
        </svg>
      </div>
      <div v-else class="auth-logo"><img src="/favicon.png" alt="BeHappy" class="auth-logo-img" /></div>
      <h1 class="auth-title">{{ isAdminHost ? 'Admin Panel' : 'BeHappy' }}</h1>
      <p class="auth-subtitle">{{ isAdminHost ? 'Authorized personnel only' : 'Sign in to your account' }}</p>

      <div v-if="error" class="alert alert-error">{{ error }}</div>

      <form @submit.prevent="handleLogin" novalidate>
        <div class="form-group">
          <label for="username">Username</label>
          <input
            id="username"
            v-model="username"
            type="text"
            autocomplete="username"
            placeholder="your_username"
            minlength="3"
            maxlength="32"
            required
          />
        </div>
        <div class="form-group">
          <label for="password">Password</label>
          <input
            id="password"
            v-model="password"
            type="password"
            autocomplete="current-password"
            placeholder="------"
            minlength="6"
            maxlength="128"
            required
          />
        </div>
        <button type="submit" :class="['btn', 'btn-full', isAdminHost ? 'btn-admin' : 'btn-primary']" :disabled="loading">
          {{ loading ? 'Signing in...' : 'Sign in' }}
        </button>
      </form>

      <p v-if="!isAdminHost" class="auth-footer">
        No account? <router-link to="/register">Register</router-link>
      </p>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const authStore = useAuthStore()

const username = ref('')
const password = ref('')
const error = ref('')
const loading = ref(false)

const isAdminHost = window.location.hostname.startsWith('admin.')

async function handleLogin() {
  error.value = ''
  loading.value = true
  try {
    await authStore.login(username.value.trim(), password.value.trim())
    if (isAdminHost && !authStore.isAdmin) {
      authStore.logout()
      error.value = 'Access denied. Admin privileges required.'
      return
    }
    router.push(isAdminHost ? '/admin' : '/app')
  } catch (e: unknown) {
    const err = e as { response?: { data?: { error?: string } } }
    error.value = err.response?.data?.error || 'Login failed.'
  } finally {
    loading.value = false
  }
}
</script>
