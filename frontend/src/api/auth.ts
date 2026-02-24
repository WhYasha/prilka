import api from './client'
import type { AuthTokens, User } from './types'

export async function login(username: string, password: string): Promise<AuthTokens> {
  const { data } = await api.post<AuthTokens>('/auth/login', { username, password })
  return data
}

export async function register(username: string, email: string, password: string): Promise<void> {
  await api.post('/auth/register', { username, email, password })
}

export async function refreshToken(refresh_token: string): Promise<AuthTokens> {
  const { data } = await api.post<AuthTokens>('/auth/refresh', { refresh_token })
  return data
}

export async function getMe(): Promise<User> {
  const { data } = await api.get<User>('/me')
  return data
}
