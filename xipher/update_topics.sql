-- Add missing columns for Telegram-style topics
ALTER TABLE group_topics ADD COLUMN IF NOT EXISTS is_hidden BOOLEAN DEFAULT false;
ALTER TABLE group_topics ADD COLUMN IF NOT EXISTS last_message TEXT DEFAULT '';
ALTER TABLE group_topics ADD COLUMN IF NOT EXISTS last_message_sender VARCHAR(255) DEFAULT '';
