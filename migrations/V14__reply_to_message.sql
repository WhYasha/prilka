-- V14: Add reply-to-message support
ALTER TABLE messages ADD COLUMN IF NOT EXISTS reply_to_message_id BIGINT;
CREATE INDEX IF NOT EXISTS idx_messages_reply_to ON messages (reply_to_message_id) WHERE reply_to_message_id IS NOT NULL;
