-- Migration: Add support for saved messages (self-chat)
-- Created: 2024-12-XX
-- Description: Removes constraint that prevents users from sending messages to themselves
--              This enables the "Saved Messages" feature where users can save messages to themselves

-- Remove the constraint that prevents self-messages
ALTER TABLE messages DROP CONSTRAINT IF EXISTS no_self_message;

-- Add a comment to document this change
COMMENT ON TABLE messages IS 'Messages table. Supports both regular chats (sender_id != receiver_id) and saved messages (sender_id == receiver_id)';

