CREATE TABLE message_reactions (
    message_id  BIGINT       NOT NULL,
    user_id     BIGINT       NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    emoji       TEXT         NOT NULL,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    PRIMARY KEY (message_id, user_id, emoji)
);
CREATE INDEX idx_message_reactions_message ON message_reactions (message_id);
CREATE INDEX idx_message_reactions_user ON message_reactions (user_id);
