-- Migration: Add public directory support for groups and channels
-- Adds category and is_public/is_verified fields for catalog functionality

-- Add columns to groups table
ALTER TABLE groups ADD COLUMN IF NOT EXISTS category VARCHAR(50) DEFAULT '';
ALTER TABLE groups ADD COLUMN IF NOT EXISTS is_public BOOLEAN DEFAULT FALSE;

-- Add columns to channels table  
ALTER TABLE channels ADD COLUMN IF NOT EXISTS category VARCHAR(50) DEFAULT '';
ALTER TABLE channels ADD COLUMN IF NOT EXISTS is_verified BOOLEAN DEFAULT FALSE;

-- Create indexes for faster public directory queries
CREATE INDEX IF NOT EXISTS idx_groups_is_public ON groups(is_public) WHERE is_public = TRUE;
CREATE INDEX IF NOT EXISTS idx_groups_category ON groups(category) WHERE category != '';
CREATE INDEX IF NOT EXISTS idx_channels_is_private ON channels(is_private) WHERE is_private = FALSE;
CREATE INDEX IF NOT EXISTS idx_channels_category ON channels(category) WHERE category != '';
CREATE INDEX IF NOT EXISTS idx_channels_is_verified ON channels(is_verified) WHERE is_verified = TRUE;

-- Comments for documentation
COMMENT ON COLUMN groups.category IS 'Category for public directory (tech, gaming, crypto, news, etc.)';
COMMENT ON COLUMN groups.is_public IS 'Whether the group appears in public directory';
COMMENT ON COLUMN channels.category IS 'Category for public directory (tech, gaming, crypto, news, etc.)';
COMMENT ON COLUMN channels.is_verified IS 'Whether the channel is verified (shows badge in catalog)';
