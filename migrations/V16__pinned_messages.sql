CREATE TABLE IF NOT EXISTS pinned_messages (
    id          BIGSERIAL PRIMARY KEY,
    chat_id     BIGINT NOT NULL REFERENCES chats(id),
    message_id  BIGINT NOT NULL REFERENCES messages(id),
    pinned_by   BIGINT NOT NULL REFERENCES users(id),
    pinned_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    unpinned_at TIMESTAMPTZ
);

CREATE INDEX idx_pinned_messages_chat ON pinned_messages(chat_id) WHERE unpinned_at IS NULL;
