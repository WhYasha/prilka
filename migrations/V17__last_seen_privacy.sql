-- V17: Add last-seen visibility privacy setting
ALTER TABLE user_settings ADD COLUMN IF NOT EXISTS last_seen_visibility TEXT NOT NULL DEFAULT 'everyone';
ALTER TABLE user_settings ADD CONSTRAINT chk_last_seen_visibility CHECK (last_seen_visibility IN ('everyone', 'nobody', 'approx_only'));
