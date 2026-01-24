-- Advanced Channels & Supergroups migration (non-destructive)
-- Aligns with unified chats model; keeps legacy tables intact.

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Enum types (create if absent)
DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'chat_type') THEN
        CREATE TYPE chat_type AS ENUM ('private','group','supergroup','channel');
    END IF;
END $$;

DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_type WHERE typname = 'participant_status') THEN
        CREATE TYPE participant_status AS ENUM ('owner','admin','member','restricted','kicked','left');
    END IF;
END $$;

-- Core chats
CREATE TABLE IF NOT EXISTS chats (
    id              UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    type            chat_type NOT NULL,
    title           VARCHAR(128) NOT NULL,
    username        VARCHAR(32) UNIQUE,
    about           TEXT,
    is_public       BOOLEAN DEFAULT FALSE,
    photo_id        UUID,
    slow_mode_sec   INTEGER CHECK (slow_mode_sec BETWEEN 0 AND 3600),
    sign_messages   BOOLEAN DEFAULT TRUE,
    created_by      UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_chats_type ON chats(type);
CREATE INDEX IF NOT EXISTS idx_chats_username ON chats(username) WHERE username IS NOT NULL;

-- Per-chat message sequence
CREATE TABLE IF NOT EXISTS chat_sequences (
    chat_id   UUID PRIMARY KEY REFERENCES chats(id) ON DELETE CASCADE,
    next_local BIGINT NOT NULL DEFAULT 1
);

-- Admin roles with bitmask perms
CREATE TABLE IF NOT EXISTS admin_roles (
    id       UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    chat_id  UUID NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    title    VARCHAR(32) DEFAULT 'Admin',
    perms    BIGINT NOT NULL,
    created_at TIMESTAMPTZ DEFAULT now(),
    UNIQUE(chat_id, title)
);

-- Participants/subscribers
CREATE TABLE IF NOT EXISTS chat_participants (
    chat_id      UUID NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    user_id      UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    status       participant_status NOT NULL,
    admin_role_id UUID REFERENCES admin_roles(id) ON DELETE SET NULL,
    joined_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
    invited_by   UUID,
    mute_until   TIMESTAMPTZ,
    PRIMARY KEY (chat_id, user_id)
);

CREATE INDEX IF NOT EXISTS idx_cp_user ON chat_participants(user_id);
CREATE INDEX IF NOT EXISTS idx_cp_admin ON chat_participants(chat_id) WHERE status IN ('admin','owner');
CREATE INDEX IF NOT EXISTS idx_cp_status ON chat_participants(chat_id, status);

-- Channel <-> discussion link (1:1)
CREATE TABLE IF NOT EXISTS chat_links (
    channel_id    UUID PRIMARY KEY REFERENCES chats(id) ON DELETE CASCADE,
    discussion_id UUID UNIQUE REFERENCES chats(id) ON DELETE SET NULL,
    created_at    TIMESTAMPTZ DEFAULT now()
);

-- Messages (global id + per-chat local id)
CREATE TABLE IF NOT EXISTS chat_messages (
    id              BIGSERIAL PRIMARY KEY,
    chat_id         UUID NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    local_id        BIGINT NOT NULL,
    sender_id       UUID,
    replied_to      BIGINT REFERENCES chat_messages(id),
    thread_root_id  BIGINT REFERENCES chat_messages(id),
    content_type    VARCHAR(32) NOT NULL,
    content         JSONB NOT NULL,
    is_silent       BOOLEAN DEFAULT FALSE,
    scheduled_at    TIMESTAMPTZ,
    created_at      TIMESTAMPTZ DEFAULT now(),
    edited_at       TIMESTAMPTZ
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_chat_messages_local ON chat_messages(chat_id, local_id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_thread ON chat_messages(thread_root_id);
CREATE INDEX IF NOT EXISTS idx_chat_messages_created ON chat_messages(chat_id, created_at);

-- Restrictions (bans/mutes with granular rights)
CREATE TABLE IF NOT EXISTS participant_restrictions (
    chat_id    UUID NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    user_id    UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    can_send_messages BOOLEAN DEFAULT FALSE,
    can_send_media    BOOLEAN DEFAULT FALSE,
    can_add_links     BOOLEAN DEFAULT FALSE,
    can_invite_users  BOOLEAN DEFAULT FALSE,
    until      TIMESTAMPTZ,
    PRIMARY KEY(chat_id, user_id)
);

-- Admin action log (event sourcing)
CREATE TABLE IF NOT EXISTS admin_log_events (
    id           BIGSERIAL PRIMARY KEY,
    chat_id      UUID NOT NULL REFERENCES chats(id) ON DELETE CASCADE,
    actor_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    target_user  UUID,
    target_msg   BIGINT,
    event_type   VARCHAR(64) NOT NULL,
    old_value    JSONB,
    new_value    JSONB,
    created_at   TIMESTAMPTZ DEFAULT now()
);
CREATE INDEX IF NOT EXISTS idx_admin_log_chat_created ON admin_log_events(chat_id, created_at);

-- Reactions (aggregate + optional shards)
CREATE TABLE IF NOT EXISTS message_reaction_counts (
    message_id  BIGINT PRIMARY KEY REFERENCES chat_messages(id) ON DELETE CASCADE,
    counts      JSONB NOT NULL
);
CREATE TABLE IF NOT EXISTS message_reactions_shard (
    shard       SMALLINT NOT NULL,
    message_id  BIGINT NOT NULL REFERENCES chat_messages(id) ON DELETE CASCADE,
    user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    reaction    VARCHAR(16) NOT NULL,
    created_at  TIMESTAMPTZ DEFAULT now(),
    PRIMARY KEY(shard, message_id, user_id)
);

-- Polls
CREATE TABLE IF NOT EXISTS polls (
    id             BIGSERIAL PRIMARY KEY,
    message_id     BIGINT UNIQUE REFERENCES chat_messages(id) ON DELETE CASCADE,
    question       TEXT NOT NULL,
    is_anonymous   BOOLEAN DEFAULT TRUE,
    allows_multiple BOOLEAN DEFAULT FALSE,
    closes_at      TIMESTAMPTZ
);
CREATE TABLE IF NOT EXISTS poll_options (
    id          BIGSERIAL PRIMARY KEY,
    poll_id     BIGINT NOT NULL REFERENCES polls(id) ON DELETE CASCADE,
    option_text TEXT NOT NULL,
    vote_count  BIGINT DEFAULT 0
);
CREATE TABLE IF NOT EXISTS poll_votes (
    poll_id     BIGINT NOT NULL REFERENCES polls(id) ON DELETE CASCADE,
    option_id   BIGINT NOT NULL REFERENCES poll_options(id) ON DELETE CASCADE,
    user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    voted_at    TIMESTAMPTZ DEFAULT now(),
    PRIMARY KEY(poll_id, user_id, option_id)
);
CREATE INDEX IF NOT EXISTS idx_poll_votes_option ON poll_votes(option_id);

-- Triggers
DROP TRIGGER IF EXISTS update_chats_updated_at ON chats;
CREATE TRIGGER update_chats_updated_at BEFORE UPDATE ON chats
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();
