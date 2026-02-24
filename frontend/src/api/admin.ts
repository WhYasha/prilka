import api from './client'
import type { AdminStats, AdminUserList, AdminUserDetail, AdminMessageList, SupportData } from './types'

export async function getStats(): Promise<AdminStats> {
  const { data } = await api.get<AdminStats>('/admin-api/stats')
  return data
}

export async function getUsers(params: {
  page?: number
  per_page?: number
  q?: string
  status?: string
}): Promise<AdminUserList> {
  const { data } = await api.get<AdminUserList>('/admin-api/users', { params })
  return data
}

export async function getUserDetail(id: number): Promise<AdminUserDetail> {
  const { data } = await api.get<AdminUserDetail>(`/admin-api/users/${id}`)
  return data
}

export async function userAction(
  id: number,
  action: 'block' | 'unblock' | 'soft-delete' | 'toggle-admin',
): Promise<void> {
  await api.post(`/admin-api/users/${id}/${action}`)
}

export async function getMessages(params: {
  page?: number
  per_page?: number
  user_id?: string
  chat_id?: string
  q?: string
}): Promise<AdminMessageList> {
  const { data } = await api.get<AdminMessageList>('/admin-api/messages', { params })
  return data
}

export async function getSupportUsers(): Promise<SupportData> {
  const { data } = await api.get<SupportData>('/admin-api/support/users')
  return data
}

export async function getSupportChats(): Promise<SupportData> {
  const { data } = await api.get<SupportData>('/admin-api/support/chats')
  return data
}

export async function getSupportMessages(): Promise<SupportData> {
  const { data } = await api.get<SupportData>('/admin-api/support/messages')
  return data
}

export async function sendSupportMessage(payload: {
  target_user_id?: number
  chat_id?: number
  content: string
}): Promise<void> {
  await api.post('/admin-api/support/send', payload)
}
