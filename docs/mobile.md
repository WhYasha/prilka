# Mobile API Contract (iOS / Swift)

This document describes the API contracts for implementing a future iOS client in Swift.

## Base URL

Development: `http://localhost:8080`
Production: `https://api.messenger.example.com`

## Authentication

All protected endpoints require:
```
Authorization: Bearer <access_token>
```

Tokens are short-lived (1h). Use `/auth/refresh` with the refresh token to get a new access token.
Store tokens securely in **Keychain**, not UserDefaults.

---

## Endpoints

### Auth

#### POST /auth/register
```json
// Request
{ "username": "alice", "email": "alice@example.com", "password": "secret123" }

// Response 201
{ "id": 1, "username": "alice" }

// Error 400
{ "error": "Username must be 3–32 characters" }
// Error 409
{ "error": "Username or email already taken" }
```

#### POST /auth/login
```json
// Request
{ "username": "alice", "password": "secret123" }

// Response 200
{
  "access_token": "eyJ...",
  "refresh_token": "eyJ...",
  "token_type": "Bearer",
  "expires_in": 3600,
  "user_id": 1
}
```

#### POST /auth/refresh
```json
// Request
{ "refresh_token": "eyJ..." }

// Response 200
{ "access_token": "eyJ...", "token_type": "Bearer", "expires_in": 3600 }
```

#### GET /me
```json
// Response 200
{
  "id": 1,
  "username": "alice",
  "email": "alice@example.com",
  "display_name": "Alice",
  "bio": "Hello world",
  "created_at": "2026-02-23T10:00:00Z"
}
```

---

### Users

#### GET /users/{id}
```json
// Response 200
{ "id": 2, "username": "bob", "display_name": "Bob", "bio": null }
```

#### GET /users/search?q=ali
```json
// Response 200
[{ "id": 1, "username": "alice", "display_name": "Alice" }]
```

---

### Chats

#### POST /chats
```json
// Request — 1:1 chat
{ "type": "direct", "member_ids": [2] }

// Request — group chat
{ "type": "group", "name": "Team Alpha", "member_ids": [2, 3, 4] }

// Response 201
{ "id": 5, "type": "created" }
```

#### GET /chats
```json
// Response 200
[{
  "id": 5,
  "type": "direct",
  "name": null,
  "updated_at": "2026-02-23T11:00:00Z",
  "last_message": "Hey there!"
}]
```

#### GET /chats/{id}
```json
// Response 200
{
  "id": 5,
  "type": "group",
  "name": "Team Alpha",
  "owner_id": 1,
  "created_at": "2026-02-23T10:00:00Z",
  "members": [
    { "id": 1, "username": "alice", "display_name": "Alice", "role": "owner" },
    { "id": 2, "username": "bob",   "display_name": "Bob",   "role": "member" }
  ]
}
```

---

### Messages

#### POST /chats/{id}/messages
```json
// Request — text
{ "content": "Hello!", "type": "text" }

// Request — file message
{ "content": "", "type": "file", "file_id": 42 }

// Response 201
{
  "id": 99,
  "chat_id": 5,
  "sender_id": 1,
  "content": "Hello!",
  "type": "text",
  "created_at": "2026-02-23T11:01:00Z"
}
```

#### GET /chats/{id}/messages?limit=50&before=99
```json
// Response 200 — newest first
[{
  "id": 99,
  "chat_id": 5,
  "sender_id": 1,
  "content": "Hello!",
  "message_type": "text",
  "created_at": "2026-02-23T11:01:00Z"
}]
```

---

### Files

#### POST /files (multipart/form-data, field name: `file`)
```json
// Response 201
{
  "id": 42,
  "filename": "photo.jpg",
  "mime_type": "image/jpeg",
  "object_key": "uuid_photo.jpg"
}
```

#### GET /files/{id}/download
```
HTTP 302 → presigned MinIO/S3 URL (valid 1 hour)
```

---

### WebSocket `/ws`

Connect with any WebSocket client. Protocol is JSON text frames.

#### Handshake (authentication)
```json
// Client → Server (first message after connect)
{ "type": "auth", "token": "<access_token>" }

// Server → Client (on success)
{ "type": "auth_ok", "user_id": 1 }

// Server → Client (on failure — connection closed)
{ "type": "error", "message": "Invalid or expired access token" }
```

#### Subscribe to a chat
```json
// Client → Server
{ "type": "subscribe", "chat_id": 5 }

// Server → Client
{ "type": "subscribed", "chat_id": 5 }
```

#### Receive messages
```json
// Server → Client (pushed when someone sends a message to a subscribed chat)
{
  "type": "message",
  "id": 99,
  "chat_id": 5,
  "sender_id": 2,
  "content": "Hey!",
  "message_type": "text",
  "created_at": "2026-02-23T11:05:00Z"
}
```

#### Keepalive
```json
// Client → Server
{ "type": "ping" }

// Server → Client
{ "type": "pong" }
```

---

## iOS Implementation Notes

- Use `URLSessionWebSocketTask` (iOS 13+) for WebSocket.
- Use `Codable` structs for all JSON models.
- Store tokens in **Keychain** via `Security` framework.
- Implement token refresh in a `URLSession` delegate / Alamofire interceptor.
- WebSocket reconnect: exponential backoff (1s, 2s, 4s … 60s cap).
- File uploads: use multipart form data with `URLRequest`.
- Presigned URLs expire in 1h — do not cache download URLs beyond their TTL.
- All timestamps are **ISO 8601 UTC** strings — decode with `ISO8601DateFormatter`.

## Planned iOS Features (future)
- Push notifications via APNs (server sends via Firebase/APNs after WS message)
- Voice messages (upload as audio/ogg or audio/m4a to `/files`)
- Read receipts: `POST /chats/{id}/messages/{msgId}/read`
- User avatar: upload to `/files`, then `PUT /me` with `{ "avatar_file_id": 42 }`
