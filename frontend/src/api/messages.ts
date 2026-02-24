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
  },
): Promise<Message> {
  const { data } = await api.post<Message>(`/chats/${chatId}/messages`, payload)
  return data
}

export async function deleteMessage(chatId: number, messageId: number): Promise<void> {
  await api.delete(`/chats/${chatId}/messages/${messageId}`)
}

export async function forwardMessages(
  toChatId: number,
  payload: { from_chat_id: number; message_ids: number[] },
): Promise<Message[]> {
  const { data } = await api.post<Message[]>(`/chats/${toChatId}/messages/forward`, payload)
  return data
}
