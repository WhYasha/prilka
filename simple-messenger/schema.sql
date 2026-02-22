-- Simple Messenger – Database Schema v2
-- Telegram-like prototype

CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    password_hash TEXT    NOT NULL,
    display_name  TEXT,
    bio           TEXT,
    avatar_path   TEXT,
    created_at    DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS conversations (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_a_id  INTEGER NOT NULL REFERENCES users(id),
    user_b_id  INTEGER NOT NULL REFERENCES users(id),
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_a_id, user_b_id)
);

CREATE INDEX IF NOT EXISTS idx_conversations_users
    ON conversations(user_a_id, user_b_id);

CREATE TABLE IF NOT EXISTS messages (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id  INTEGER NOT NULL REFERENCES conversations(id),
    from_user_id     INTEGER NOT NULL REFERENCES users(id),
    content          TEXT,
    message_type     TEXT NOT NULL DEFAULT 'text',  -- text | sticker | voice
    attachment_path  TEXT,
    duration_seconds INTEGER,
    created_at       DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_messages_conv
    ON messages(conversation_id, id);

CREATE TABLE IF NOT EXISTS user_settings (
    user_id              INTEGER PRIMARY KEY REFERENCES users(id),
    theme                TEXT    NOT NULL DEFAULT 'light',
    notifications_enabled INTEGER NOT NULL DEFAULT 1,
    language             TEXT    NOT NULL DEFAULT 'en'
);

CREATE TABLE IF NOT EXISTS stickers (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    pack_name TEXT NOT NULL,
    label     TEXT NOT NULL,
    file_path TEXT NOT NULL
);
