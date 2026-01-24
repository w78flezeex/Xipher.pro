-- Migration: Add file, voice, emoji and reply support to messages
-- Created: 2024-12-03

-- Add new columns to messages table
ALTER TABLE messages 
    ADD COLUMN IF NOT EXISTS message_type VARCHAR(20) DEFAULT 'text' CHECK (message_type IN ('text', 'file', 'voice', 'image', 'emoji', 'location', 'live_location')),
    ADD COLUMN IF NOT EXISTS file_path VARCHAR(500),
    ADD COLUMN IF NOT EXISTS file_name VARCHAR(255),
    ADD COLUMN IF NOT EXISTS file_size BIGINT,
    ADD COLUMN IF NOT EXISTS reply_to_message_id UUID REFERENCES messages(id) ON DELETE SET NULL;

-- Create index for reply_to_message_id for faster lookups
CREATE INDEX IF NOT EXISTS idx_messages_reply_to ON messages(reply_to_message_id);

-- Update existing messages to have message_type = 'text'
UPDATE messages SET message_type = 'text' WHERE message_type IS NULL;
