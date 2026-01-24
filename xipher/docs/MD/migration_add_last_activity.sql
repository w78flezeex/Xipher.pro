-- Migration: Add last_activity field to users table
-- Created: 2024-12-XX
-- Description: Adds last_activity timestamp field to track user online status

-- Add last_activity column to users table
ALTER TABLE users ADD COLUMN IF NOT EXISTS last_activity TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP;

-- Create index for faster queries on last_activity
CREATE INDEX IF NOT EXISTS idx_users_last_activity ON users(last_activity);

-- Update existing users to set last_activity to current timestamp
UPDATE users SET last_activity = CURRENT_TIMESTAMP WHERE last_activity IS NULL;

