-- V1: Initial schema for production messenger
-- PostgreSQL — prepared for partitioning on messages.created_at

-- --------------------------------------------------------------------------
-- EXTENSIONS
-- --------------------------------------------------------------------------
CREATE EXTENSION IF NOT EXISTS pgcrypto;   -- gen_random_uuid()
CREATE EXTENSION IF NOT EXISTS citext;     -- case-insensitive text

-- --------------------------------------------------------------------------
-- USERS
-- --------------------------------------------------------------------------
CREATE TABLE users (
    id           BIGSERIAL     PRIMARY KEY,
    username     CITEXT        NOT NULL UNIQUE,
    email        CITEXT        NOT NULL UNIQUE,
    password_hash TEXT         NOT NULL,
    display_name TEXT,
    bio          TEXT,
    avatar_file_id BIGINT,                -- FK added after files table created
    is_active    BOOLEAN       NOT NULL DEFAULT TRUE,
    created_at   TIMESTAMPTZ   NOT NULL DEFAULT NOW(),
    updated_at   TIMESTAMPTZ   NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_users_username ON users (username);
CREATE INDEX idx_users_email    ON users (email);

-- --------------------------------------------------------------------------
-- CHATS
-- --------------------------------------------------------------------------
CREATE TYPE chat_type AS ENUM ('direct', 'group');

CREATE TABLE chats (
    id           BIGSERIAL    PRIMARY KEY,
    type         chat_type    NOT NULL DEFAULT 'direct',
    name         TEXT,
    owner_id     BIGINT       NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at   TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at   TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_chats_owner ON chats (owner_id);

-- --------------------------------------------------------------------------
-- CHAT MEMBERS
-- --------------------------------------------------------------------------
CREATE TABLE chat_members (
    chat_id    BIGINT       NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    user_id    BIGINT       NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    joined_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    role       TEXT         NOT NULL DEFAULT 'member',  -- 'owner' | 'admin' | 'member'
    PRIMARY KEY (chat_id, user_id)
);

CREATE INDEX idx_chat_members_user ON chat_members (user_id);

-- --------------------------------------------------------------------------
-- FILES (metadata)
-- --------------------------------------------------------------------------
CREATE TABLE files (
    id           BIGSERIAL    PRIMARY KEY,
    uploader_id  BIGINT       NOT NULL REFERENCES users(id),
    bucket       TEXT         NOT NULL,
    object_key   TEXT         NOT NULL UNIQUE,
    filename     TEXT         NOT NULL,
    mime_type    TEXT,
    size_bytes   BIGINT,
    created_at   TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_files_uploader ON files (uploader_id);

-- Add deferred FK for avatar (files table now exists)
ALTER TABLE users ADD CONSTRAINT fk_users_avatar
    FOREIGN KEY (avatar_file_id) REFERENCES files(id) ON DELETE SET NULL;

-- --------------------------------------------------------------------------
-- MESSAGES
-- Partitioning: declare as partitioned by range on created_at.
-- Initial partition covers all data; partition by month in production.
-- --------------------------------------------------------------------------
CREATE TABLE messages (
    id              BIGSERIAL,
    chat_id         BIGINT       NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    sender_id       BIGINT       NOT NULL REFERENCES users(id),
    content         TEXT,
    message_type    TEXT         NOT NULL DEFAULT 'text',  -- 'text' | 'file' | 'sticker'
    file_id         BIGINT       REFERENCES files(id) ON DELETE SET NULL,
    created_at      TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    PRIMARY KEY (id, created_at)
) PARTITION BY RANGE (created_at);

-- Initial catch-all partition
CREATE TABLE messages_default
    PARTITION OF messages DEFAULT;

-- Monthly partitions for 2026 — extend each month via migration
CREATE TABLE messages_2026_01
    PARTITION OF messages
    FOR VALUES FROM ('2026-01-01') TO ('2026-02-01');

CREATE TABLE messages_2026_02
    PARTITION OF messages
    FOR VALUES FROM ('2026-02-01') TO ('2026-03-01');

CREATE TABLE messages_2026_03
    PARTITION OF messages
    FOR VALUES FROM ('2026-03-01') TO ('2026-04-01');

CREATE INDEX idx_messages_chat_created ON messages (chat_id, created_at DESC);
CREATE INDEX idx_messages_sender       ON messages (sender_id);

-- --------------------------------------------------------------------------
-- MESSAGE READ RECEIPTS
-- --------------------------------------------------------------------------
CREATE TABLE message_read_receipts (
    message_id  BIGINT       NOT NULL,
    user_id     BIGINT       NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    read_at     TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    PRIMARY KEY (message_id, user_id)
    -- Note: no FK on message_id because messages is partitioned;
    -- referential integrity is enforced at application level
);

CREATE INDEX idx_read_receipts_user ON message_read_receipts (user_id);

-- --------------------------------------------------------------------------
-- REFRESH TOKENS
-- --------------------------------------------------------------------------
CREATE TABLE refresh_tokens (
    id          BIGSERIAL    PRIMARY KEY,
    user_id     BIGINT       NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash  TEXT         NOT NULL UNIQUE,
    expires_at  TIMESTAMPTZ  NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    revoked     BOOLEAN      NOT NULL DEFAULT FALSE
);

CREATE INDEX idx_refresh_tokens_user    ON refresh_tokens (user_id);
CREATE INDEX idx_refresh_tokens_hash    ON refresh_tokens (token_hash);
CREATE INDEX idx_refresh_tokens_expires ON refresh_tokens (expires_at)
    WHERE revoked = FALSE;

-- --------------------------------------------------------------------------
-- USER SETTINGS
-- --------------------------------------------------------------------------
CREATE TABLE user_settings (
    user_id               BIGINT  PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    theme                 TEXT    NOT NULL DEFAULT 'light',
    notifications_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    language              TEXT    NOT NULL DEFAULT 'en'
);

-- --------------------------------------------------------------------------
-- update updated_at trigger
-- --------------------------------------------------------------------------
CREATE OR REPLACE FUNCTION set_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_users_updated_at
    BEFORE UPDATE ON users
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();

CREATE TRIGGER trg_chats_updated_at
    BEFORE UPDATE ON chats
    FOR EACH ROW EXECUTE FUNCTION set_updated_at();
