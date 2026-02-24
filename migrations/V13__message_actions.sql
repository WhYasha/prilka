-- V13: Message actions — delete, forward, pin chat, archive chat
-- Supports soft-delete (for everyone) and per-user delete, message forwarding,
-- chat pinning, and chat archiving

-- --------------------------------------------------------------------------
-- Soft-delete flag on messages (delete for everyone)
-- --------------------------------------------------------------------------
ALTER TABLE messages ADD COLUMN IF NOT EXISTS is_deleted BOOLEAN NOT NULL DEFAULT FALSE;

-- --------------------------------------------------------------------------
-- Per-user deleted messages (delete for me)
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS deleted_messages (
    user_id    BIGINT      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    message_id BIGINT      NOT NULL,
    deleted_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, message_id)
    -- Note: no FK on message_id because messages is partitioned
);
CREATE INDEX ON deleted_messages(user_id);

-- --------------------------------------------------------------------------
-- Forwarded-from metadata on messages
-- --------------------------------------------------------------------------
ALTER TABLE messages ADD COLUMN IF NOT EXISTS forwarded_from_chat_id    BIGINT REFERENCES chats(id) ON DELETE SET NULL;
ALTER TABLE messages ADD COLUMN IF NOT EXISTS forwarded_from_message_id BIGINT;

-- --------------------------------------------------------------------------
-- Pinned chats per user
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS pinned_chats (
    user_id    BIGINT NOT NULL REFERENCES users(id)  ON DELETE CASCADE,
    chat_id    BIGINT NOT NULL REFERENCES chats(id)  ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, chat_id)
);
CREATE INDEX ON pinned_chats(user_id);

-- --------------------------------------------------------------------------
-- Archived chats per user
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS archived_chats (
    user_id    BIGINT NOT NULL REFERENCES users(id)  ON DELETE CASCADE,
    chat_id    BIGINT NOT NULL REFERENCES chats(id)  ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, chat_id)
);
CREATE INDEX ON archived_chats(user_id);
