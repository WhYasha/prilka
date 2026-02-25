import api from './client'
import type { FileUploadResult } from './types'

export async function uploadFile(file: File): Promise<FileUploadResult> {
  const fd = new FormData()
  fd.append('file', file)
  const { data } = await api.post<FileUploadResult>('/files', fd)
  return data
}

export function getDownloadUrl(fileId: number): string {
  const base = import.meta.env.VITE_API_URL || '/api'
  return `${base}/files/${fileId}/download`
}
