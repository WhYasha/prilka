-- V8: Admin panel support
-- Adds is_admin, is_blocked, last_activity columns to users table
-- Creates bh_support system user for admin-initiated support messages

-- --------------------------------------------------------------------------
-- Admin flag (defaults to FALSE for all existing users)
-- --------------------------------------------------------------------------
ALTER TABLE users ADD COLUMN IF NOT EXISTS is_admin BOOLEAN NOT NULL DEFAULT FALSE;

-- --------------------------------------------------------------------------
-- Blocked flag (soft-block: user cannot login, messages preserved)
-- --------------------------------------------------------------------------
ALTER TABLE users ADD COLUMN IF NOT EXISTS is_blocked BOOLEAN NOT NULL DEFAULT FALSE;

-- --------------------------------------------------------------------------
-- Last activity timestamp — updated on login and WebSocket auth
-- --------------------------------------------------------------------------
ALTER TABLE users ADD COLUMN IF NOT EXISTS last_activity TIMESTAMPTZ;

-- --------------------------------------------------------------------------
-- Indexes for admin dashboard queries
-- --------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_users_is_admin      ON users (is_admin) WHERE is_admin = TRUE;
CREATE INDEX IF NOT EXISTS idx_users_last_activity  ON users (last_activity) WHERE last_activity IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_users_created_at     ON users (created_at);

-- --------------------------------------------------------------------------
-- System support user (used for admin-initiated support messages)
-- Password hash 'SYSTEM_ACCOUNT_NO_LOGIN' can never match PBKDF2 output,
-- so this account cannot be used for direct login.
-- --------------------------------------------------------------------------
INSERT INTO users (username, email, password_hash, display_name, bio, is_admin)
VALUES (
    'bh_support',
    'support@behappy.rest',
    'SYSTEM_ACCOUNT_NO_LOGIN',
    'BH Support',
    'Official support account for BH Messenger',
    TRUE
)
ON CONFLICT (username) DO NOTHING;

-- --------------------------------------------------------------------------
-- Grant admin to 'alice' seed user for dev convenience
-- --------------------------------------------------------------------------
UPDATE users SET is_admin = TRUE WHERE username = 'alice';
