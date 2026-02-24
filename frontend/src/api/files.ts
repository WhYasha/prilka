import api from './client'
import type { FileUploadResult } from './types'

export async function uploadFile(file: File): Promise<FileUploadResult> {
  const fd = new FormData()
  fd.append('file', file)
  const { data } = await api.post<FileUploadResult>('/files', fd)
  return data
}

export function getDownloadUrl(fileId: number): string {
  return `/api/files/${fileId}/download`
}
