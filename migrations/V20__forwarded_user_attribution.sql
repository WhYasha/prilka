-- V20: Add original sender attribution for forwarded messages
ALTER TABLE messages ADD COLUMN IF NOT EXISTS forwarded_from_user_id BIGINT REFERENCES users(id) ON DELETE SET NULL;
ALTER TABLE messages ADD COLUMN IF NOT EXISTS forwarded_from_display_name TEXT;
