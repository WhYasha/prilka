import api from './client'
import type { AvatarResult, User } from './types'

export async function getUser(id: number): Promise<User> {
  const { data } = await api.get<User>(`/users/${id}`)
  return data
}

export async function getUserByUsername(username: string): Promise<User> {
  const { data } = await api.get<User>(`/users/by-username/${encodeURIComponent(username)}`)
  return data
}

export async function searchUsers(q: string): Promise<User[]> {
  const { data } = await api.get<User[]>('/users/search', { params: { q } })
  return data
}

export async function updateUser(
  id: number,
  payload: { display_name?: string; bio?: string; username?: string },
): Promise<User> {
  const { data } = await api.put<User>(`/users/${id}`, payload)
  return data
}

export async function uploadAvatar(fileId: number): Promise<AvatarResult> {
  const { data } = await api.put<AvatarResult>('/users/me/avatar', { file_id: fileId })
  return data
}
