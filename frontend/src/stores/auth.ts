import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import * as authApi from '@/api/auth'
import type { User } from '@/api/types'

export const useAuthStore = defineStore('auth', () => {
  const user = ref<User | null>(null)
  const accessToken = ref<string | null>(localStorage.getItem('access_token'))
  const refreshTokenValue = ref<string | null>(localStorage.getItem('refresh_token'))

  const isLoggedIn = computed(() => !!accessToken.value)
  const isAdmin = computed(() => !!user.value?.is_admin)

  async function login(username: string, password: string) {
    const tokens = await authApi.login(username, password)
    accessToken.value = tokens.access_token
    localStorage.setItem('access_token', tokens.access_token)
    if (tokens.refresh_token) {
      refreshTokenValue.value = tokens.refresh_token
      localStorage.setItem('refresh_token', tokens.refresh_token)
    }
    await fetchMe()
  }

  async function register(username: string, email: string, password: string) {
    await authApi.register(username, email, password)
    // Auto-login after registration
    await login(username, password)
  }

  async function fetchMe() {
    try {
      user.value = await authApi.getMe()
    } catch {
      user.value = null
      // Clear stale token so isLoggedIn reflects reality
      accessToken.value = null
      refreshTokenValue.value = null
      localStorage.removeItem('access_token')
      localStorage.removeItem('refresh_token')
    }
  }

  function logout() {
    user.value = null
    accessToken.value = null
    refreshTokenValue.value = null
    localStorage.removeItem('access_token')
    localStorage.removeItem('refresh_token')
  }

  function updateUser(updates: Partial<User>) {
    if (user.value) {
      Object.assign(user.value, updates)
    }
  }

  return {
    user,
    accessToken,
    refreshTokenValue,
    isLoggedIn,
    isAdmin,
    login,
    register,
    fetchMe,
    logout,
    updateUser,
  }
})
