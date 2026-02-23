-- V6: Add 'channel' to chat_type enum
-- Separate migration because PostgreSQL requires new enum values
-- to be committed before they can be referenced in DDL/DML.

ALTER TYPE chat_type ADD VALUE IF NOT EXISTS 'channel';
