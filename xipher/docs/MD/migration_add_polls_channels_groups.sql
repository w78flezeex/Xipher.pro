-- Migration: Add support for polls in channels and groups
-- This migration updates the polls table to support polls in channels and groups,
-- not just chat_messages

-- Step 1: Add message_type column to polls table
ALTER TABLE polls ADD COLUMN IF NOT EXISTS message_type VARCHAR(20) DEFAULT 'chat';
-- message_type can be: 'chat', 'channel', 'group'

-- Step 2: Drop the unique constraint on message_id (since we'll have different message types)
ALTER TABLE polls DROP CONSTRAINT IF EXISTS polls_message_id_key;

-- Step 3: Drop the foreign key constraint on chat_messages
ALTER TABLE polls DROP CONSTRAINT IF EXISTS polls_message_id_fkey;

-- Step 4: Add a unique constraint on (message_type, message_id) to ensure one poll per message
ALTER TABLE polls ADD CONSTRAINT polls_message_type_id_unique UNIQUE (message_type, message_id);

-- Step 5: Update existing polls to have message_type = 'chat'
UPDATE polls SET message_type = 'chat' WHERE message_type IS NULL OR message_type = '';

-- Step 6: Create index for faster lookups
CREATE INDEX IF NOT EXISTS idx_polls_message_type_id ON polls(message_type, message_id);

-- Step 7: Add comment for documentation
COMMENT ON COLUMN polls.message_type IS 'Type of message: chat, channel, or group';
COMMENT ON TABLE polls IS 'Polls can be attached to chat_messages, channel_messages, or group_messages';

