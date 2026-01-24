-- Backfill channel owners as admins (v2 unified chats + legacy channels)
-- Safe to run multiple times.

BEGIN;

-- 1) Unified chats (v2): ensure creator is owner participant
WITH missing_owner AS (
    SELECT c.id AS chat_id, c.created_by AS user_id
    FROM chats c
    WHERE c.type = 'channel'
      AND NOT EXISTS (
          SELECT 1 FROM chat_participants cp
          WHERE cp.chat_id = c.id
            AND cp.user_id = c.created_by
            AND cp.status IN ('owner','admin')
      )
)
INSERT INTO chat_participants (chat_id, user_id, status)
SELECT chat_id, user_id, 'owner' FROM missing_owner
ON CONFLICT (chat_id, user_id)
DO UPDATE SET status = 'owner', admin_role_id = NULL;

-- 2) Ensure Owner admin role exists with full permissions (0xFF)
WITH upsert_roles AS (
    INSERT INTO admin_roles (chat_id, title, perms)
    SELECT c.id, 'Owner', 255
    FROM chats c
    WHERE c.type = 'channel'
    ON CONFLICT (chat_id, title) DO UPDATE SET perms = EXCLUDED.perms
    RETURNING chat_id, id AS role_id
)
UPDATE chat_participants cp
SET status = 'owner', admin_role_id = ur.role_id
FROM upsert_roles ur
JOIN chats c ON c.id = ur.chat_id
WHERE cp.chat_id = c.id
  AND cp.user_id = c.created_by;

-- 3) Legacy channels: ensure creator row exists and marked as creator
INSERT INTO channel_members (channel_id, user_id, role, is_banned)
SELECT ch.id, ch.creator_id, 'creator', FALSE
FROM channels ch
WHERE NOT EXISTS (
    SELECT 1 FROM channel_members m
    WHERE m.channel_id = ch.id AND m.user_id = ch.creator_id
)
ON CONFLICT (channel_id, user_id)
DO UPDATE SET role = 'creator', is_banned = FALSE;

-- 4) Targeted fix for @svortex (both schemas)
-- Replace 'svortex' if the handle differs.
DO $$
DECLARE
    v_chat_id UUID;
    v_legacy_id UUID;
BEGIN
    SELECT id INTO v_chat_id FROM chats WHERE username = 'svortex' AND type = 'channel';
    IF v_chat_id IS NOT NULL THEN
        INSERT INTO chat_participants (chat_id, user_id, status)
        SELECT v_chat_id, c.created_by, 'owner'
        FROM chats c WHERE c.id = v_chat_id
        ON CONFLICT (chat_id, user_id) DO UPDATE SET status = 'owner', admin_role_id = NULL;
    END IF;

    SELECT id INTO v_legacy_id FROM channels WHERE custom_link = 'svortex';
    IF v_legacy_id IS NOT NULL THEN
        INSERT INTO channel_members (channel_id, user_id, role, is_banned)
        SELECT v_legacy_id, c.creator_id, 'creator', FALSE
        FROM channels c WHERE c.id = v_legacy_id
        ON CONFLICT (channel_id, user_id) DO UPDATE SET role = 'creator', is_banned = FALSE;
    END IF;
END $$;

COMMIT;

