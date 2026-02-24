import api from './client'
import type { Chat } from './types'

export async function listChats(): Promise<Chat[]> {
  const { data } = await api.get<Chat[]>('/chats')
  return data
}

export async function getChat(id: number): Promise<Chat> {
  const { data } = await api.get<Chat>(`/chats/${id}`)
  return data
}

export async function getChatByName(name: string): Promise<Chat> {
  const { data } = await api.get<Chat>(`/chats/by-name/${encodeURIComponent(name)}`)
  return data
}

export async function createChat(payload: {
  type: string
  title?: string
  description?: string
  public_name?: string
  member_ids: number[]
}): Promise<Chat> {
  const { data } = await api.post<Chat>('/chats', payload)
  return data
}

export async function favoriteChat(id: number): Promise<void> {
  await api.post(`/chats/${id}/favorite`)
}

export async function unfavoriteChat(id: number): Promise<void> {
  await api.delete(`/chats/${id}/favorite`)
}

export async function muteChat(id: number, muted_until?: string): Promise<void> {
  await api.post(`/chats/${id}/mute`, muted_until ? { muted_until } : {})
}

export async function unmuteChat(id: number): Promise<void> {
  await api.delete(`/chats/${id}/mute`)
}

export async function leaveChat(id: number): Promise<void> {
  await api.delete(`/chats/${id}/leave`)
}

export async function markRead(id: number): Promise<void> {
  await api.post(`/chats/${id}/read`)
}

export async function pinChat(id: number): Promise<void> {
  await api.post(`/chats/${id}/pin`)
}

export async function unpinChat(id: number): Promise<void> {
  await api.delete(`/chats/${id}/pin`)
}

export async function archiveChat(id: number): Promise<void> {
  await api.post(`/chats/${id}/archive`)
}

export async function unarchiveChat(id: number): Promise<void> {
  await api.delete(`/chats/${id}/archive`)
}
