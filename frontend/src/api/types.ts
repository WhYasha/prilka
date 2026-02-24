export interface User {
  id: number
  username: string
  display_name: string
  bio: string
  avatar_url: string | null
  email?: string
  is_admin: boolean
  is_active?: boolean
  is_blocked?: boolean
  created_at?: string
  updated_at?: string
  last_activity?: string
}

export interface AuthTokens {
  access_token: string
  refresh_token?: string
}

export interface Chat {
  id: number
  type: 'direct' | 'group' | 'channel'
  name: string | null
  title: string | null
  description?: string
  public_name?: string
  other_user_id?: number
  other_username?: string
  other_display_name?: string
  other_avatar_url?: string | null
  is_favorite: boolean
  is_muted: boolean
  is_pinned?: boolean
  is_archived?: boolean
  unread_count: number
  last_message?: string
  last_at?: string
  updated_at: string
  member_count?: number
  avatar_url?: string | null
  my_role?: string
}

export interface ReactionGroup {
  emoji: string
  count: number
  me: boolean
}

export type ReactionsMap = Record<string, ReactionGroup[]>

export interface Message {
  id: number
  chat_id: number
  sender_id: number
  sender_username: string
  sender_display_name: string
  sender_avatar_url: string | null
  sender_is_admin: boolean
  content: string
  message_type: 'text' | 'sticker' | 'voice' | 'file'
  sticker_id?: number
  sticker_url?: string
  sticker_label?: string
  attachment_url?: string
  attachment_path?: string
  duration_seconds?: number
  file_id?: number
  forwarded_from_chat_id?: number
  forwarded_from_message_id?: number
  reply_to_message_id?: number
  reply_to_content?: string
  reply_to_type?: string
  reply_to_sender_username?: string
  reply_to_sender_name?: string
  is_edited?: boolean
  updated_at?: string
  created_at: string
  reactions?: ReactionGroup[]
}

export interface Sticker {
  id: number
  label: string
  url: string
}

export interface Settings {
  theme: string
  notifications_enabled: boolean
  language: string
  last_seen_visibility: 'everyone' | 'approx_only' | 'nobody'
}

export interface Invite {
  id: number
  token: string
  chat_id: number
  created_by: number
  expires_at?: string
  created_at: string
}

export interface InvitePreview {
  title: string
  type: string
  description: string
  member_count: number
}

export interface FileUploadResult {
  id: number
  filename: string
  url?: string
}

export interface AvatarResult {
  avatar_url: string
}

export interface ChatMember {
  id: number
  username: string
  display_name: string
  avatar_url: string | null
  role: 'owner' | 'admin' | 'member'
  joined_at: string
}

// Admin types
export interface AdminStats {
  total_users: number
  dau: number
  wau: number
  new_users_today: number
  total_chats: number
  total_messages: number
  messages_today: number
  blocked_users: number
  chats_by_type: Record<string, number>
  reg_trend: Array<{ day: string; count: number }>
  msg_trend: Array<{ day: string; count: number }>
}

export interface AdminUserList {
  users: User[]
  page: number
  total_pages: number
  total: number
}

export interface AdminUserDetail {
  user: User
  chats: Array<{
    id: number
    type: string
    name: string
    title: string
    role: string
    member_count: number
    joined_at: string
  }>
  messages: Array<{
    id: number
    chat_id: number
    chat_name: string
    chat_type: string
    message_type: string
    content: string
    created_at: string
  }>
  message_count: number
}

export interface AdminMessageList {
  messages: Array<{
    id: number
    chat_id: number
    chat_name: string
    chat_type: string
    sender_id: number
    sender_username: string
    message_type: string
    content: string
    created_at: string
  }>
  page: number
  total_pages: number
  total: number
}

export interface SupportData {
  users?: User[]
  chats?: Array<{
    id: number
    type: string
    name: string
    title: string
    member_count: number
  }>
  messages?: Array<{
    id: number
    chat_id: number
    chat_name: string
    chat_type: string
    content: string
    created_at: string
  }>
}
