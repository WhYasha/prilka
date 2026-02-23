-- V7: Add channel metadata columns to chats + create invites table
-- Depends on V6 which added 'channel' to chat_type enum.

-- --------------------------------------------------------------------------
-- CHATS: channel metadata
-- --------------------------------------------------------------------------
ALTER TABLE chats
    ADD COLUMN title       TEXT,
    ADD COLUMN description TEXT,
    ADD COLUMN public_name CITEXT UNIQUE;

-- --------------------------------------------------------------------------
-- INVITES
-- --------------------------------------------------------------------------
CREATE TABLE invites (
    id          BIGSERIAL    PRIMARY KEY,
    chat_id     BIGINT       NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    token       UUID         NOT NULL UNIQUE DEFAULT gen_random_uuid(),
    created_by  BIGINT       NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    revoked_at  TIMESTAMPTZ
);

CREATE INDEX idx_invites_chat  ON invites (chat_id);
CREATE INDEX idx_invites_token ON invites (token);

-- Partial index: only active (non-revoked) invites
CREATE INDEX idx_invites_active ON invites (chat_id, token)
    WHERE revoked_at IS NULL;
