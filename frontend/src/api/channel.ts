import api from './client'
import type { Chat, ChatMember } from './types'

export async function getChatMembers(chatId: number): Promise<ChatMember[]> {
  const { data } = await api.get<Chat & { members: ChatMember[] }>(`/chats/${chatId}`)
  return data.members ?? []
}

export async function updateChat(
  chatId: number,
  payload: { title?: string; description?: string },
): Promise<Chat> {
  // TODO: backend PUT /chats/{id} endpoint needed
  const { data } = await api.put<Chat>(`/chats/${chatId}`, payload)
  return data
}

export async function uploadChatAvatar(
  chatId: number,
  fileId: string,
): Promise<void> {
  // TODO: requires backend support
  await api.post(`/chats/${chatId}/avatar`, { file_id: fileId })
}
