-- Channels Broadcast v3 (PTS + roles + invites + stats)
-- Non-destructive: adds new tables/columns/triggers, keeps legacy data intact.

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Enum for participant status (owner/admin/subscriber/banned)
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'channel_participant_status') THEN
        CREATE TYPE channel_participant_status AS ENUM ('owner','admin','subscriber','banned');
    END IF;
END
$$;

-- Channels: extend metadata for public/private & avatar/hash
ALTER TABLE channels
    ADD COLUMN IF NOT EXISTS access_hash UUID DEFAULT uuid_generate_v4(),
    ADD COLUMN IF NOT EXISTS username VARCHAR(32) UNIQUE,
    ADD COLUMN IF NOT EXISTS avatar_url TEXT,
    ADD COLUMN IF NOT EXISTS owner_id UUID,
    ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMPTZ;

-- Backfill owner_id from creator_id when present
UPDATE channels SET owner_id = creator_id WHERE owner_id IS NULL AND creator_id IS NOT NULL;

-- Participants with perms bitmask and status
ALTER TABLE channel_members
    ADD COLUMN IF NOT EXISTS permissions BIGINT DEFAULT 0,
    ADD COLUMN IF NOT EXISTS invited_by UUID,
    ADD COLUMN IF NOT EXISTS status channel_participant_status DEFAULT 'subscriber',
    ADD COLUMN IF NOT EXISTS muted_until TIMESTAMPTZ;

-- Admin roles per channel (bitmask)
CREATE TABLE IF NOT EXISTS channel_admin_roles (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    channel_id UUID NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    title VARCHAR(32) DEFAULT 'Admin',
    perms BIGINT NOT NULL,
    created_at TIMESTAMPTZ DEFAULT now(),
    UNIQUE(channel_id, title)
);

-- Invite links (public handles use channels.username)
CREATE TABLE IF NOT EXISTS channel_invite_links (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    channel_id UUID NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    token VARCHAR(64) NOT NULL UNIQUE,
    expire_at TIMESTAMPTZ,
    usage_limit INT,
    usage_count INT DEFAULT 0,
    is_revoked BOOLEAN DEFAULT FALSE,
    created_by UUID NOT NULL,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- Per-channel message sequence (PTS)
CREATE TABLE IF NOT EXISTS channel_sequences (
    channel_id UUID PRIMARY KEY REFERENCES channels(id) ON DELETE CASCADE,
    next_local BIGINT NOT NULL DEFAULT 1
);

-- Function to assign next local_id
CREATE OR REPLACE FUNCTION next_channel_local_id(p_channel_id UUID)
RETURNS BIGINT AS $$
DECLARE
    v_next BIGINT;
BEGIN
    UPDATE channel_sequences
    SET next_local = next_local + 1
    WHERE channel_id = p_channel_id
    RETURNING next_local - 1 INTO v_next;

    IF NOT FOUND THEN
        INSERT INTO channel_sequences(channel_id, next_local)
        VALUES (p_channel_id, 2)
        ON CONFLICT (channel_id) DO NOTHING
        RETURNING 1 INTO v_next;

        IF v_next IS NULL THEN
            -- Another concurrent insert happened, retry once
            RETURN next_channel_local_id(p_channel_id);
        END IF;
    END IF;

    RETURN v_next;
END;
$$ LANGUAGE plpgsql;

-- Messages: add per-channel sequence, rich content, scheduling, soft delete
ALTER TABLE channel_messages
    ADD COLUMN IF NOT EXISTS local_id BIGINT,
    ADD COLUMN IF NOT EXISTS content_json JSONB,
    ADD COLUMN IF NOT EXISTS media_id UUID,
    ADD COLUMN IF NOT EXISTS is_silent BOOLEAN DEFAULT FALSE,
    ADD COLUMN IF NOT EXISTS scheduled_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS edited_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS deleted_at TIMESTAMPTZ,
    ADD COLUMN IF NOT EXISTS thread_root BIGINT,
    ADD COLUMN IF NOT EXISTS reactions JSONB DEFAULT '{}'::jsonb;

-- Unique local sequence per channel
CREATE UNIQUE INDEX IF NOT EXISTS idx_channel_messages_local ON channel_messages(channel_id, local_id);
CREATE INDEX IF NOT EXISTS idx_channel_messages_sched ON channel_messages(scheduled_at) WHERE scheduled_at IS NOT NULL;

-- Trigger to set local_id & default JSON content
CREATE OR REPLACE FUNCTION channel_messages_local_id_trigger()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.local_id IS NULL THEN
        NEW.local_id := next_channel_local_id(NEW.channel_id);
    END IF;
    IF NEW.content_json IS NULL THEN
        NEW.content_json := jsonb_build_object('text', NEW.content);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_channel_messages_local_id ON channel_messages;
CREATE TRIGGER trg_channel_messages_local_id
BEFORE INSERT ON channel_messages
FOR EACH ROW EXECUTE FUNCTION channel_messages_local_id_trigger();

-- Read states per user/channel
CREATE TABLE IF NOT EXISTS channel_read_states (
    channel_id UUID NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    max_read_local BIGINT DEFAULT 0,
    updated_at TIMESTAMPTZ DEFAULT now(),
    PRIMARY KEY(channel_id, user_id)
);
CREATE INDEX IF NOT EXISTS idx_crs_user_channel ON channel_read_states(user_id, channel_id);

-- Message stats (views/reactions aggregate; optional HLL blob)
CREATE TABLE IF NOT EXISTS channel_message_stats (
    message_id UUID PRIMARY KEY REFERENCES channel_messages(id) ON DELETE CASCADE,
    views BIGINT DEFAULT 0,
    reactions JSONB DEFAULT '{}'::jsonb,
    hll BYTEA
);

-- Optional sharded user-level reactions (dedupe)
CREATE TABLE IF NOT EXISTS channel_message_reactions_shard (
    shard SMALLINT NOT NULL,
    message_id UUID NOT NULL REFERENCES channel_messages(id) ON DELETE CASCADE,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    reaction VARCHAR(16) NOT NULL,
    created_at TIMESTAMPTZ DEFAULT now(),
    PRIMARY KEY (shard, message_id, user_id)
);

-- Discussion link 1:1
CREATE TABLE IF NOT EXISTS channel_discussion_links (
    channel_id UUID PRIMARY KEY REFERENCES channels(id) ON DELETE CASCADE,
    group_id UUID UNIQUE REFERENCES groups(id) ON DELETE SET NULL,
    created_at TIMESTAMPTZ DEFAULT now()
);

-- Scheduled jobs for delayed posts
CREATE TYPE channel_job_status AS ENUM ('pending','published','canceled');
CREATE TABLE IF NOT EXISTS channel_scheduled_jobs (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    channel_id UUID NOT NULL REFERENCES channels(id) ON DELETE CASCADE,
    message_payload JSONB NOT NULL,
    scheduled_at TIMESTAMPTZ NOT NULL,
    status channel_job_status DEFAULT 'pending',
    created_at TIMESTAMPTZ DEFAULT now(),
    UNIQUE(channel_id, id)
);
CREATE INDEX IF NOT EXISTS idx_channel_jobs_sched ON channel_scheduled_jobs(scheduled_at) WHERE status = 'pending';

-- Activity log index refinement
CREATE INDEX IF NOT EXISTS idx_channel_activity_log_created ON channel_activity_log(channel_id, created_at);

-- Helpful view for unread counts
CREATE OR REPLACE VIEW channel_unread_view AS
SELECT
    c.id AS channel_id,
    rs.user_id,
    (COALESCE(seq.next_local, 1) - 1) - COALESCE(rs.max_read_local, 0) AS unread_count
FROM channels c
LEFT JOIN channel_sequences seq ON seq.channel_id = c.id
LEFT JOIN channel_read_states rs ON rs.channel_id = c.id;
