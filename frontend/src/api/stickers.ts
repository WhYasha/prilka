import api from './client'
import type { Sticker } from './types'

export async function listStickers(): Promise<Sticker[]> {
  const { data } = await api.get<Sticker[]>('/stickers')
  return data
}
