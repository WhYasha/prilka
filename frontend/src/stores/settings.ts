import { defineStore } from 'pinia'
import { ref } from 'vue'
import * as settingsApi from '@/api/settings'
import type { Settings } from '@/api/types'

export const useSettingsStore = defineStore('settings', () => {
  const theme = ref(localStorage.getItem('theme') || 'light')
  const notificationsEnabled = ref(true)
  const language = ref('en')
  const lastSeenVisibility = ref<'everyone' | 'approx_only' | 'nobody'>('everyone')
  const readReceiptsEnabled = ref(true)

  function applyTheme(t: string) {
    theme.value = t
    document.body.setAttribute('data-theme', t)
    localStorage.setItem('theme', t)
  }

  async function loadSettings() {
    try {
      const s = await settingsApi.getSettings()
      applyTheme(s.theme || 'light')
      notificationsEnabled.value = s.notifications_enabled
      language.value = s.language || 'en'
      lastSeenVisibility.value = s.last_seen_visibility || 'everyone'
      readReceiptsEnabled.value = s.read_receipts_enabled !== false
    } catch (e) {
      console.error('Failed to load settings', e)
    }
  }

  async function saveSettings(s: Partial<Settings>) {
    await settingsApi.putSettings(s)
    if (s.theme) applyTheme(s.theme)
    if (s.notifications_enabled !== undefined) notificationsEnabled.value = s.notifications_enabled
    if (s.language) language.value = s.language
    if (s.last_seen_visibility) lastSeenVisibility.value = s.last_seen_visibility
    if (s.read_receipts_enabled !== undefined) readReceiptsEnabled.value = s.read_receipts_enabled
  }

  // Apply theme on init
  document.body.setAttribute('data-theme', theme.value)

  return {
    theme,
    notificationsEnabled,
    language,
    lastSeenVisibility,
    readReceiptsEnabled,
    applyTheme,
    loadSettings,
    saveSettings,
  }
})
