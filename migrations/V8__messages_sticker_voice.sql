-- V8: Add sticker_id and duration_seconds to messages
-- These are needed by the C++ API for sticker and voice message support.
-- Messages is a partitioned table; ALTER TABLE propagates to all partitions in PG16.

ALTER TABLE messages
    ADD COLUMN IF NOT EXISTS sticker_id       BIGINT REFERENCES stickers(id) ON DELETE SET NULL,
    ADD COLUMN IF NOT EXISTS duration_seconds INTEGER;

CREATE INDEX IF NOT EXISTS idx_messages_sticker ON messages (sticker_id)
    WHERE sticker_id IS NOT NULL;
