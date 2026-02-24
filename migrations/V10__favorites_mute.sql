-- V10: Favorites and mute settings per user
-- Allows users to star/favorite chats and mute notifications per chat

-- --------------------------------------------------------------------------
-- Starred/favorite chats per user
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chat_favorites (
    user_id    BIGINT NOT NULL REFERENCES users(id)  ON DELETE CASCADE,
    chat_id    BIGINT NOT NULL REFERENCES chats(id)  ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, chat_id)
);
CREATE INDEX ON chat_favorites(user_id);

-- --------------------------------------------------------------------------
-- Per-user chat mute settings
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chat_mute_settings (
    user_id     BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    chat_id     BIGINT NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    muted_until TIMESTAMPTZ,   -- NULL = muted indefinitely
    PRIMARY KEY (user_id, chat_id)
);
CREATE INDEX ON chat_mute_settings(user_id);
