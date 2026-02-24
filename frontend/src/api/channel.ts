import api from './client'

export async function uploadChatAvatar(
  chatId: number,
  fileId: number,
): Promise<void> {
  await api.post(`/chats/${chatId}/avatar`, { file_id: fileId })
}

export async function promoteMember(
  chatId: number,
  userId: number,
): Promise<{ role: string }> {
  const resp = await api.post(`/chats/${chatId}/members/${userId}/promote`)
  return resp.data
}

export async function demoteMember(
  chatId: number,
  userId: number,
): Promise<{ role: string }> {
  const resp = await api.post(`/chats/${chatId}/members/${userId}/demote`)
  return resp.data
}
