-- V17: Add avatar_file_id column to chats table for group/channel avatars
ALTER TABLE chats ADD COLUMN avatar_file_id BIGINT REFERENCES files(id) ON DELETE SET NULL;
