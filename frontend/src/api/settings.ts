import api from './client'
import type { Settings } from './types'

export async function getSettings(): Promise<Settings> {
  const { data } = await api.get<Settings>('/settings')
  return data
}

export async function putSettings(settings: Partial<Settings>): Promise<void> {
  await api.put('/settings', settings)
}
