#!/usr/bin/env bash
# Fix channel owners and apply schema for Xipher PostgreSQL.
# Run this script as a user that has access to postgres (e.g. root with sudo, or directly as postgres).
set -euo pipefail

DB_NAME="${DB_NAME:-xipher}"
PSQL_BIN="${PSQL_BIN:-psql}"
APP_DB_USER="${APP_DB_USER:-${XIPHER_DB_USER:-xipher}}"

# Avoid permission issues on /root cwd when switching user
cd /

# Helper to run psql with sudo -u postgres when available.
run_psql() {
  local cmd="$1"
  if command -v sudo >/dev/null 2>&1; then
    sudo -u postgres ${PSQL_BIN} -v ON_ERROR_STOP=1 -d "${DB_NAME}" -c "${cmd}"
  else
    ${PSQL_BIN} -v ON_ERROR_STOP=1 -d "${DB_NAME}" -c "${cmd}"
  fi
}

# Helper to run psql -f file (schema)
run_psql_file() {
  local file="$1"
  if command -v sudo >/dev/null 2>&1; then
    sudo -u postgres ${PSQL_BIN} -v ON_ERROR_STOP=1 -d "${DB_NAME}" -f "${file}"
  else
    ${PSQL_BIN} -v ON_ERROR_STOP=1 -d "${DB_NAME}" -f "${file}"
  fi
}

main() {
  echo "[*] Applying schema/migrations to ${DB_NAME}"
  run_psql_file "/root/xipher/docs/MD/schema.sql"
  run_psql_file "/root/xipher/docs/MD/groups_schema.sql"
  run_psql_file "/root/xipher/docs/MD/channels_schema.sql"
  run_psql_file "/root/xipher/docs/MD/migration_channels_supergroups_v2.sql"

  # v2 pins (chat_messages does not have is_pinned; we store it in chat_pins)
  run_psql "CREATE TABLE IF NOT EXISTS chat_pins (
    chat_id    UUID PRIMARY KEY REFERENCES chats(id) ON DELETE CASCADE,
    message_id BIGINT REFERENCES chat_messages(id) ON DELETE CASCADE,
    pinned_by  UUID REFERENCES users(id) ON DELETE SET NULL,
    pinned_at  TIMESTAMPTZ NOT NULL DEFAULT now()
  );"

  # Direct dialog pins (legacy messages table)
  run_psql "CREATE TABLE IF NOT EXISTS direct_pins (
    user_low UUID NOT NULL,
    user_high UUID NOT NULL,
    message_id UUID NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
    pinned_by UUID NOT NULL REFERENCES users(id) ON DELETE SET NULL,
    pinned_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_low, user_high)
  );"

  # Moderation reports (message_id stored as text to support UUID and bigint IDs)
  run_psql "CREATE TABLE IF NOT EXISTS message_reports (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    reporter_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    reported_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    message_id TEXT NOT NULL,
    message_type VARCHAR(32) NOT NULL DEFAULT 'direct',
    message_content TEXT NOT NULL,
    reason VARCHAR(32) NOT NULL,
    comment TEXT,
    context_json JSONB DEFAULT '[]'::jsonb,
    status VARCHAR(16) NOT NULL DEFAULT 'pending',
    resolution_note TEXT,
    resolved_by UUID REFERENCES users(id) ON DELETE SET NULL,
    resolved_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
  );"

  # Ensure profile/privacy tables exist even if schema.sql was not updated in an old deployment
  run_psql "CREATE TABLE IF NOT EXISTS user_profiles (
    user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    first_name VARCHAR(70) DEFAULT '',
    last_name VARCHAR(70) DEFAULT '',
    bio TEXT DEFAULT '',
    birth_day SMALLINT,
    birth_month SMALLINT,
    birth_year SMALLINT,
    personal_color VARCHAR(32) DEFAULT '',
    linked_channel_id UUID,
    business_hours_json JSONB,
    updated_at TIMESTAMPTZ DEFAULT now()
  );"

  run_psql "CREATE TABLE IF NOT EXISTS user_privacy_settings (
    user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    bio_visibility VARCHAR(16) NOT NULL DEFAULT 'everyone',
    birth_visibility VARCHAR(16) NOT NULL DEFAULT 'contacts',
    last_seen_visibility VARCHAR(16) NOT NULL DEFAULT 'contacts',
    avatar_visibility VARCHAR(16) NOT NULL DEFAULT 'everyone',
    updated_at TIMESTAMPTZ DEFAULT now()
  );"

  echo "[*] Ensuring DB privileges for app user: ${APP_DB_USER}"
  run_psql "GRANT USAGE ON SCHEMA public TO \"${APP_DB_USER}\";"
  run_psql "GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO \"${APP_DB_USER}\";"
  run_psql "GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO \"${APP_DB_USER}\";"
  run_psql "ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO \"${APP_DB_USER}\";"
  run_psql "ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT USAGE, SELECT ON SEQUENCES TO \"${APP_DB_USER}\";"

  echo "[*] Fixing channel owners and roles"
  run_psql "WITH chan AS (SELECT id, created_by FROM chats WHERE type='channel')
    INSERT INTO chat_participants (chat_id, user_id, status)
    SELECT id, created_by, 'owner' FROM chan
    ON CONFLICT (chat_id,user_id) DO UPDATE SET status='owner';"

  run_psql "INSERT INTO admin_roles (chat_id, title, perms)
    SELECT id, 'Owner', 255 FROM chats WHERE type='channel'
    ON CONFLICT (chat_id, title) DO UPDATE SET perms=255;"

  run_psql "UPDATE chat_participants cp
    SET admin_role_id = ar.id
    FROM admin_roles ar, chats c
    WHERE cp.chat_id = c.id AND c.type='channel'
      AND cp.user_id = c.created_by
      AND ar.chat_id = c.id AND ar.title = 'Owner';"

  echo "[+] Done. Verify with: sudo -u postgres psql -d ${DB_NAME} -c \"SELECT count(*) FROM chat_participants WHERE status='owner';\""
}

main "$@"
