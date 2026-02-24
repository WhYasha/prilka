<template>
  <div class="auth-page">
    <div class="auth-card">
      <div class="auth-logo">&#128172;</div>
      <h1 class="auth-title">Simple Messenger</h1>
      <p class="auth-subtitle">Sign in to your account</p>

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
        <button type="submit" class="btn btn-primary btn-full" :disabled="loading">
          {{ loading ? 'Signing in...' : 'Sign in' }}
        </button>
      </form>

      <p class="auth-footer">
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

async function handleLogin() {
  error.value = ''
  loading.value = true
  try {
    await authStore.login(username.value.trim(), password.value)
    router.push('/app')
  } catch (e: unknown) {
    const err = e as { response?: { data?: { error?: string } } }
    error.value = err.response?.data?.error || 'Login failed.'
  } finally {
    loading.value = false
  }
}
</script>
