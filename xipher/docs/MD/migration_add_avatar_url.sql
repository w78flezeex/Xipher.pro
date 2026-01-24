-- Migration: Add avatar_url field to users table
-- Created: 2024-12-XX
-- Description: Adds avatar_url field to store user profile picture path

-- Add avatar_url column to users table
ALTER TABLE users ADD COLUMN IF NOT EXISTS avatar_url VARCHAR(500);

-- Create index for faster queries on avatar_url (optional, but useful if searching by avatar)
-- CREATE INDEX IF NOT EXISTS idx_users_avatar_url ON users(avatar_url);
