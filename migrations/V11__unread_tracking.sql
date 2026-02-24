-- Track per-user read position in each chat for unread count calculation.
-- Stores the last message ID the user has seen in a given chat.

CREATE TABLE chat_last_read (
    user_id    BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    chat_id    BIGINT NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    last_read_msg_id BIGINT NOT NULL DEFAULT 0,
    read_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, chat_id)
);

CREATE INDEX idx_chat_last_read_chat ON chat_last_read (chat_id);
