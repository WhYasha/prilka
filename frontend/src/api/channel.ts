import api from './client'

export async function uploadChatAvatar(
  chatId: number,
  fileId: number,
): Promise<void> {
  await api.post(`/chats/${chatId}/avatar`, { file_id: fileId })
}
