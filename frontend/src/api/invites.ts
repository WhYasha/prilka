import api from './client'
import type { Invite, InvitePreview } from './types'

export async function createInvite(chatId: number): Promise<Invite> {
  const { data } = await api.post<Invite>(`/chats/${chatId}/invites`)
  return data
}

export async function listInvites(chatId: number): Promise<Invite[]> {
  const { data } = await api.get<Invite[]>(`/chats/${chatId}/invites`)
  return data
}

export async function revokeInvite(token: string): Promise<void> {
  await api.delete(`/invites/${encodeURIComponent(token)}`)
}

export async function previewInvite(token: string): Promise<InvitePreview> {
  const { data } = await api.get<InvitePreview>(
    `/invites/${encodeURIComponent(token)}/preview`,
  )
  return data
}

export async function joinInvite(
  token: string,
): Promise<{ chat_id: number; already_member: boolean }> {
  const { data } = await api.post<{ chat_id: number; already_member: boolean }>(
    `/invites/${encodeURIComponent(token)}/join`,
  )
  return data
}
