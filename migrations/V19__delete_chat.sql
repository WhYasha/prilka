-- V19: Fix pinned_messages FK to allow chat deletion via CASCADE

-- pinned_messages.chat_id was missing ON DELETE CASCADE (V16),
-- which blocks DELETE FROM chats when pinned messages exist.
ALTER TABLE pinned_messages DROP CONSTRAINT pinned_messages_chat_id_fkey;
ALTER TABLE pinned_messages ADD CONSTRAINT pinned_messages_chat_id_fkey
    FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE;
