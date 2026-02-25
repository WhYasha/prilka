import api from './client'
import type { Message } from './types'

export async function listMessages(
  chatId: number,
  params?: { after_id?: number; before?: string; limit?: number },
): Promise<Message[]> {
  const { data } = await api.get<Message[]>(`/chats/${chatId}/messages`, { params })
  return data
}

export async function sendMessage(
  chatId: number,
  payload: {
    content: string
    type: string
    sticker_id?: number
    file_id?: number
    duration_seconds?: number
    reply_to_message_id?: number
  },
): Promise<Message> {
  const { data } = await api.post<Message>(`/chats/${chatId}/messages`, payload)
  return data
}

export async function deleteMessage(chatId: number, messageId: number, forEveryone = false): Promise<void> {
  await api.delete(`/chats/${chatId}/messages/${messageId}`, forEveryone ? { data: { for_everyone: true } } : undefined)
}

export async function editMessage(chatId: number, messageId: number, content: string): Promise<Message> {
  const { data } = await api.put<Message>(`/chats/${chatId}/messages/${messageId}`, { content })
  return data
}

export async function forwardMessages(
  toChatId: number,
  payload: { from_chat_id: number; message_ids: number[] },
): Promise<Message[]> {
  const { data } = await api.post<Message[]>(`/chats/${toChatId}/messages/forward`, payload)
  return data
}

export async function pinMessage(chatId: number, messageId: number) {
  const { data } = await api.post(`/chats/${chatId}/messages/${messageId}/pin`)
  return data
}

export async function unpinMessage(chatId: number, messageId: number) {
  await api.delete(`/chats/${chatId}/messages/${messageId}/pin`)
}

export async function getPinnedMessage(chatId: number) {
  const { data } = await api.get(`/chats/${chatId}/pinned-message`)
  return data
}

export async function searchMessages(
  chatId: number,
  q: string,
  params?: { limit?: number; before_id?: number },
): Promise<Message[]> {
  const { data } = await api.get<Message[]>(`/chats/${chatId}/messages/search`, {
    params: { q, ...params },
  })
  return data
}
