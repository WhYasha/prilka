import api from './client'
import type { ReactionsMap } from './types'

export async function toggleReaction(chatId: number, messageId: number, emoji: string) {
  const { data } = await api.post(`/chats/${chatId}/messages/${messageId}/reactions`, { emoji })
  return data as { message_id: number; emoji: string; action: 'added' | 'removed' }
}

export async function getReactions(chatId: number, messageIds: number[]): Promise<ReactionsMap> {
  if (!messageIds.length) return {}
  const { data } = await api.get<ReactionsMap>(`/chats/${chatId}/reactions`, {
    params: { message_ids: messageIds.join(',') },
  })
  return data
}
