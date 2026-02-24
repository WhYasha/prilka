<template>
  <div class="auth-page">
    <div class="auth-card">
      <div class="auth-logo">&#128172;</div>
      <h1 class="auth-title">Simple Messenger</h1>
      <p class="auth-subtitle">Create a new account</p>

      <div v-if="error" class="alert alert-error">{{ error }}</div>

      <form @submit.prevent="handleRegister" novalidate>
        <div class="form-group">
          <label for="username">Username</label>
          <input
            id="username"
            v-model="username"
            type="text"
            autocomplete="username"
            placeholder="choose_a_username"
            minlength="3"
            maxlength="32"
            required
          />
          <span class="form-hint">3-32 chars, letters/digits/- _</span>
        </div>
        <div class="form-group">
          <label for="email">Email</label>
          <input
            id="email"
            v-model="email"
            type="email"
            autocomplete="email"
            placeholder="you@example.com"
            maxlength="128"
            required
          />
        </div>
        <div class="form-group">
          <label for="password">Password</label>
          <input
            id="password"
            v-model="password"
            type="password"
            autocomplete="new-password"
            placeholder="at least 6 characters"
            minlength="6"
            maxlength="128"
            required
          />
        </div>
        <button type="submit" class="btn btn-primary btn-full" :disabled="loading">
          {{ loading ? 'Creating...' : 'Create account' }}
        </button>
      </form>

      <p class="auth-footer">
        Already have an account? <router-link to="/login">Sign in</router-link>
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
const email = ref('')
const password = ref('')
const error = ref('')
const loading = ref(false)

async function handleRegister() {
  error.value = ''
  loading.value = true
  try {
    await authStore.register(username.value.trim(), email.value.trim(), password.value)
    router.push('/app')
  } catch (e: unknown) {
    const err = e as { response?: { data?: { error?: string } } }
    error.value = err.response?.data?.error || 'Registration failed.'
  } finally {
    loading.value = false
  }
}
</script>
