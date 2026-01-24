#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include "../include/utils/json_parser.hpp"
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <stdexcept>
#include <cstdint>

namespace xipher {

namespace {
constexpr uint64_t kAllAdminPerms = 0xFFu; // owner full permissions
}

DatabaseManager::DatabaseManager(const std::string& host,
                                const std::string& port,
                                const std::string& dbname,
                                const std::string& user,
                                const std::string& password) {
    db_ = std::make_unique<DatabaseConnection>(host, port, dbname, user, password);
}

bool DatabaseManager::initialize() {
    if (!db_->connect()) {
        Logger::getInstance().error("Failed to connect to database");
        return false;
    }

    // Make sure saved messages (self-chat) are allowed even if the DB was created
    // before the dedicated migration was applied.
    PGresult* savedMsgConstraint = db_->executeQuery("ALTER TABLE messages DROP CONSTRAINT IF EXISTS no_self_message");
    if (!savedMsgConstraint) {
        Logger::getInstance().warning("Could not drop no_self_message constraint (it may already be absent): " + db_->getLastError());
    } else {
        PQclear(savedMsgConstraint);
        Logger::getInstance().info("Ensured no_self_message constraint is dropped for saved messages support");
    }

    PGresult* messageTypeConstraint = db_->executeQuery(
        "DO $$ "
        "DECLARE c record; "
        "BEGIN "
        "  IF to_regclass('public.messages') IS NOT NULL THEN "
        "    IF EXISTS (SELECT 1 FROM information_schema.columns "
        "               WHERE table_name = 'messages' AND column_name = 'message_type') THEN "
        "      FOR c IN "
        "        SELECT conname FROM pg_constraint "
        "        WHERE conrelid = 'public.messages'::regclass "
        "          AND contype = 'c' "
        "          AND pg_get_constraintdef(oid) ILIKE '%message_type%' "
        "      LOOP "
        "        EXECUTE format('ALTER TABLE public.messages DROP CONSTRAINT IF EXISTS %I', c.conname); "
        "      END LOOP; "
        "      EXECUTE 'ALTER TABLE public.messages ADD CONSTRAINT messages_message_type_check "
        "               CHECK (message_type IN (''text'',''file'',''voice'',''image'',''emoji'',''location'',''live_location''))'; "
        "    END IF; "
        "  END IF; "
        "END $$;");
    if (!messageTypeConstraint) {
        Logger::getInstance().warning("Could not update messages.message_type constraint: " + db_->getLastError());
    } else {
        PQclear(messageTypeConstraint);
        Logger::getInstance().info("Ensured messages.message_type allows location types");
    }

    // v2 pinned message support (non-destructive). We try to create it only if v2 core tables exist.
    PGresult* pinsTable = db_->executeQuery(
        "DO $$ BEGIN "
        "IF to_regclass('public.chats') IS NOT NULL AND to_regclass('public.chat_messages') IS NOT NULL THEN "
        "EXECUTE 'CREATE TABLE IF NOT EXISTS chat_pins ("
        "  chat_id   UUID PRIMARY KEY REFERENCES chats(id) ON DELETE CASCADE,"
        "  message_id BIGINT REFERENCES chat_messages(id) ON DELETE CASCADE,"
        "  pinned_by UUID REFERENCES users(id) ON DELETE SET NULL,"
        "  pinned_at TIMESTAMPTZ NOT NULL DEFAULT now()"
        ")'; "
        "END IF; "
        "END $$;");
    if (!pinsTable) {
        Logger::getInstance().warning("Could not ensure chat_pins table exists: " + db_->getLastError());
    } else {
        PQclear(pinsTable);
    }

    PGresult* chatPinsTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS user_chat_pins ("
        "  user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  chat_type VARCHAR(16) NOT NULL,"
        "  chat_id UUID NOT NULL,"
        "  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "  PRIMARY KEY (user_id, chat_type, chat_id)"
        ")");
    if (!chatPinsTable) {
        Logger::getInstance().warning("Could not ensure user_chat_pins table exists: " + db_->getLastError());
    } else {
        PQclear(chatPinsTable);
    }
    PGresult* chatPinsIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_user_chat_pins_user ON user_chat_pins (user_id)");
    if (chatPinsIdx) PQclear(chatPinsIdx);

    PGresult* chatFoldersTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS user_chat_folders ("
        "  user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,"
        "  folders JSONB NOT NULL DEFAULT '[]'::jsonb,"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()"
        ")");
    if (!chatFoldersTable) {
        Logger::getInstance().warning("Could not ensure user_chat_folders table exists: " + db_->getLastError());
    } else {
        PQclear(chatFoldersTable);
    }

    PGresult* linkedChannelCol = db_->executeQuery(
        "ALTER TABLE IF EXISTS user_profiles "
        "ADD COLUMN IF NOT EXISTS linked_channel_id UUID");
    if (!linkedChannelCol) {
        Logger::getInstance().warning("Could not ensure user_profiles.linked_channel_id exists: " + db_->getLastError());
    } else {
        PQclear(linkedChannelCol);
    }

    PGresult* businessHoursCol = db_->executeQuery(
        "ALTER TABLE IF EXISTS user_profiles "
        "ADD COLUMN IF NOT EXISTS business_hours_json JSONB");
    if (!businessHoursCol) {
        Logger::getInstance().warning("Could not ensure user_profiles.business_hours_json exists: " + db_->getLastError());
    } else {
        PQclear(businessHoursCol);
    }

    PGresult* legacyChannelAvatarCol = db_->executeQuery(
        "ALTER TABLE IF EXISTS channels "
        "ADD COLUMN IF NOT EXISTS avatar_url VARCHAR(500)");
    if (!legacyChannelAvatarCol) {
        Logger::getInstance().warning("Could not ensure channels.avatar_url exists: " + db_->getLastError());
    } else {
        PQclear(legacyChannelAvatarCol);
    }

    PGresult* chatAvatarCol = db_->executeQuery(
        "DO $$ BEGIN "
        "IF to_regclass('public.chats') IS NOT NULL THEN "
        "EXECUTE 'ALTER TABLE chats ADD COLUMN IF NOT EXISTS avatar_url VARCHAR(500)'; "
        "END IF; "
        "END $$;");
    if (!chatAvatarCol) {
        Logger::getInstance().warning("Could not ensure chats.avatar_url exists: " + db_->getLastError());
    } else {
        PQclear(chatAvatarCol);
    }

    // Add is_verified column to chats table for verified channels
    PGresult* chatVerifiedCol = db_->executeQuery(
        "DO $$ BEGIN "
        "IF to_regclass('public.chats') IS NOT NULL THEN "
        "EXECUTE 'ALTER TABLE chats ADD COLUMN IF NOT EXISTS is_verified BOOLEAN DEFAULT FALSE'; "
        "END IF; "
        "END $$;");
    if (!chatVerifiedCol) {
        Logger::getInstance().warning("Could not ensure chats.is_verified exists: " + db_->getLastError());
    } else {
        PQclear(chatVerifiedCol);
    }

    PGresult* chatJoinRequestsTable = db_->executeQuery(
        "DO $$ BEGIN "
        "IF to_regclass('public.chats') IS NOT NULL THEN "
        "EXECUTE 'CREATE TABLE IF NOT EXISTS chat_join_requests ("
        "  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  chat_id uuid NOT NULL REFERENCES chats(id) ON DELETE CASCADE,"
        "  user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  status varchar(20) DEFAULT ''pending'' CHECK (status IN (''pending'',''accepted'',''rejected'')),"
        "  created_at timestamptz DEFAULT now(),"
        "  UNIQUE(chat_id, user_id)"
        ")'; "
        "EXECUTE 'CREATE INDEX IF NOT EXISTS idx_chat_join_requests_chat ON chat_join_requests(chat_id)'; "
        "END IF; "
        "END $$;");
    if (!chatJoinRequestsTable) {
        Logger::getInstance().warning("Could not ensure chat_join_requests table exists: " + db_->getLastError());
    } else {
        PQclear(chatJoinRequestsTable);
    }

    PGresult* chatAllowedReactionsTable = db_->executeQuery(
        "DO $$ BEGIN "
        "IF to_regclass('public.chats') IS NOT NULL THEN "
        "EXECUTE 'CREATE TABLE IF NOT EXISTS chat_allowed_reactions ("
        "  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  chat_id uuid NOT NULL REFERENCES chats(id) ON DELETE CASCADE,"
        "  reaction varchar(50) NOT NULL,"
        "  UNIQUE(chat_id, reaction)"
        ")'; "
        "EXECUTE 'CREATE INDEX IF NOT EXISTS idx_chat_allowed_reactions_chat ON chat_allowed_reactions(chat_id)'; "
        "END IF; "
        "END $$;");
    if (!chatAllowedReactionsTable) {
        Logger::getInstance().warning("Could not ensure chat_allowed_reactions table exists: " + db_->getLastError());
    } else {
        PQclear(chatAllowedReactionsTable);
    }

    PGresult* chatInviteLinksTable = db_->executeQuery(
        "DO $$ BEGIN "
        "IF to_regclass('public.chats') IS NOT NULL THEN "
        "EXECUTE 'CREATE TABLE IF NOT EXISTS chat_invite_links ("
        "  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  chat_id uuid NOT NULL REFERENCES chats(id) ON DELETE CASCADE,"
        "  token varchar(64) NOT NULL UNIQUE,"
        "  expire_at timestamptz,"
        "  usage_limit int,"
        "  usage_count int DEFAULT 0,"
        "  is_revoked boolean DEFAULT FALSE,"
        "  created_by uuid NOT NULL,"
        "  created_at timestamptz DEFAULT now()"
        ")'; "
        "EXECUTE 'CREATE INDEX IF NOT EXISTS idx_chat_invite_links_chat ON chat_invite_links(chat_id)'; "
        "END IF; "
        "END $$;");
    if (!chatInviteLinksTable) {
        Logger::getInstance().warning("Could not ensure chat_invite_links table exists: " + db_->getLastError());
    } else {
        PQclear(chatInviteLinksTable);
    }
    
    // Bot Builder: profile fields (non-destructive).
    // Use ALTER TABLE IF EXISTS to avoid failing on older deployments that don't have bot_builder_bots.
    // NOTE: keep these statements simple (no DO/EXECUTE quoting) to avoid migration syntax issues.
    PGresult* botBuilderCol1 = db_->executeQuery(
        "ALTER TABLE IF EXISTS bot_builder_bots "
        "ADD COLUMN IF NOT EXISTS bot_description TEXT DEFAULT ''");
    if (!botBuilderCol1) {
        Logger::getInstance().warning("Could not ensure bot_builder_bots.bot_description exists: " + db_->getLastError());
    } else {
        PQclear(botBuilderCol1);
    }
    PGresult* botBuilderCol2 = db_->executeQuery(
        "ALTER TABLE IF EXISTS bot_builder_bots "
        "ADD COLUMN IF NOT EXISTS bot_avatar_url VARCHAR(500) DEFAULT ''");
    if (!botBuilderCol2) {
        Logger::getInstance().warning("Could not ensure bot_builder_bots.bot_avatar_url exists: " + db_->getLastError());
    } else {
        PQclear(botBuilderCol2);
    }
    PGresult* botBuilderCol3 = db_->executeQuery(
        "ALTER TABLE IF EXISTS bot_builder_bots "
        "ADD COLUMN IF NOT EXISTS bot_user_id UUID");
    if (!botBuilderCol3) {
        Logger::getInstance().warning("Could not ensure bot_builder_bots.bot_user_id exists: " + db_->getLastError());
    } else {
        PQclear(botBuilderCol3);
    }

    // Bot Builder: bot scripts (Python) (non-destructive)
    PGresult* botScriptCol1 = db_->executeQuery(
        "ALTER TABLE IF EXISTS bot_builder_bots "
        "ADD COLUMN IF NOT EXISTS script_lang VARCHAR(16) DEFAULT 'lite'");
    if (!botScriptCol1) {
        Logger::getInstance().warning("Could not ensure bot_builder_bots.script_lang exists: " + db_->getLastError());
    } else {
        PQclear(botScriptCol1);
    }
    PGresult* botScriptCol2 = db_->executeQuery(
        "ALTER TABLE IF EXISTS bot_builder_bots "
        "ADD COLUMN IF NOT EXISTS script_enabled BOOLEAN DEFAULT FALSE");
    if (!botScriptCol2) {
        Logger::getInstance().warning("Could not ensure bot_builder_bots.script_enabled exists: " + db_->getLastError());
    } else {
        PQclear(botScriptCol2);
    }
    PGresult* botScriptCol3 = db_->executeQuery(
        "ALTER TABLE IF EXISTS bot_builder_bots "
        "ADD COLUMN IF NOT EXISTS script_code TEXT DEFAULT ''");
    if (!botScriptCol3) {
        Logger::getInstance().warning("Could not ensure bot_builder_bots.script_code exists: " + db_->getLastError());
    } else {
        PQclear(botScriptCol3);
    }

    // Bot runtime tables (non-destructive)
    PGresult* botNotesTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS bot_notes ("
        "  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  bot_user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  scope_type varchar(16) NOT NULL,"
        "  scope_id uuid NOT NULL,"
        "  note_key text NOT NULL,"
        "  note_value text NOT NULL,"
        "  created_by uuid REFERENCES users(id) ON DELETE SET NULL,"
        "  created_at timestamptz NOT NULL DEFAULT now(),"
        "  updated_at timestamptz NOT NULL DEFAULT now(),"
        "  UNIQUE (bot_user_id, scope_type, scope_id, note_key)"
        ")");
    if (!botNotesTable) {
        Logger::getInstance().warning("Could not ensure bot_notes table exists: " + db_->getLastError());
    } else {
        PQclear(botNotesTable);
    }
    PGresult* botNotesIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_bot_notes_scope "
        "ON bot_notes (bot_user_id, scope_type, scope_id)");
    if (botNotesIdx) PQclear(botNotesIdx);

    PGresult* botRemindersTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS bot_reminders ("
        "  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  bot_user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  scope_type varchar(16) NOT NULL,"
        "  scope_id uuid NOT NULL,"
        "  target_user_id uuid REFERENCES users(id) ON DELETE SET NULL,"
        "  reminder_text text NOT NULL,"
        "  due_at timestamptz NOT NULL,"
        "  sent_at timestamptz,"
        "  created_at timestamptz NOT NULL DEFAULT now()"
        ")");
    if (!botRemindersTable) {
        Logger::getInstance().warning("Could not ensure bot_reminders table exists: " + db_->getLastError());
    } else {
        PQclear(botRemindersTable);
    }
    PGresult* botRemindersIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_bot_reminders_due "
        "ON bot_reminders (due_at) WHERE sent_at IS NULL");
    if (botRemindersIdx) PQclear(botRemindersIdx);

    // Bot developers (collaboration) table
    PGresult* botDevelopersTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS bot_developers ("
        "  bot_id uuid NOT NULL REFERENCES bot_builder_bots(id) ON DELETE CASCADE,"
        "  developer_user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  added_by_user_id uuid REFERENCES users(id) ON DELETE SET NULL,"
        "  added_at timestamptz NOT NULL DEFAULT now(),"
        "  PRIMARY KEY (bot_id, developer_user_id)"
        ")");
    if (!botDevelopersTable) {
        Logger::getInstance().warning("Could not ensure bot_developers table exists: " + db_->getLastError());
    } else {
        PQclear(botDevelopersTable);
    }
    PGresult* botDevelopersIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_bot_developers_bot "
        "ON bot_developers (bot_id)");
    if (botDevelopersIdx) PQclear(botDevelopersIdx);
    PGresult* botDevelopersUserIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_bot_developers_user "
        "ON bot_developers (developer_user_id)");
    if (botDevelopersUserIdx) PQclear(botDevelopersUserIdx);

    // Bot project files (for IDE)
    PGresult* botFilesTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS bot_files ("
        "  id uuid PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  bot_id uuid NOT NULL REFERENCES bot_builder_bots(id) ON DELETE CASCADE,"
        "  file_path text NOT NULL,"
        "  file_content text NOT NULL,"
        "  created_at timestamptz NOT NULL DEFAULT now(),"
        "  updated_at timestamptz NOT NULL DEFAULT now(),"
        "  UNIQUE (bot_id, file_path)"
        ")");
    if (!botFilesTable) {
        Logger::getInstance().warning("Could not ensure bot_files table exists: " + db_->getLastError());
    } else {
        PQclear(botFilesTable);
    }
    PGresult* botFilesIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_bot_files_bot "
        "ON bot_files (bot_id)");
    if (botFilesIdx) PQclear(botFilesIdx);

    // Messages: reply_markup (inline keyboard buttons) support (non-destructive)
    PGresult* messagesReplyMarkup = db_->executeQuery(
        "ALTER TABLE IF EXISTS messages "
        "ADD COLUMN IF NOT EXISTS reply_markup JSONB DEFAULT NULL");
    if (!messagesReplyMarkup) {
        Logger::getInstance().warning("Could not ensure messages.reply_markup exists: " + db_->getLastError());
    } else {
        PQclear(messagesReplyMarkup);
    }

    // Users: is_bot flag (non-destructive). Needed for bot-user backfill below.
    PGresult* usersBotCol = db_->executeQuery(
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS is_bot BOOLEAN DEFAULT FALSE");
    if (!usersBotCol) {
        Logger::getInstance().warning("Could not ensure users.is_bot column exists: " + db_->getLastError());
    } else {
        PQclear(usersBotCol);
    }

    PGresult* usersPremiumCols = db_->executeQuery(
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS is_premium BOOLEAN DEFAULT FALSE, "
        "ADD COLUMN IF NOT EXISTS premium_plan VARCHAR(16) DEFAULT '', "
        "ADD COLUMN IF NOT EXISTS premium_expires_at TIMESTAMPTZ");
    if (!usersPremiumCols) {
        Logger::getInstance().warning("Could not ensure users premium columns exist: " + db_->getLastError());
    } else {
        PQclear(usersPremiumCols);
    }

    PGresult* premiumPaymentsTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS premium_payments ("
        "  id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),"
        "  user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  plan VARCHAR(16) NOT NULL,"
        "  amount NUMERIC(10, 2) NOT NULL,"
        "  label VARCHAR(64) NOT NULL UNIQUE,"
        "  status VARCHAR(16) NOT NULL DEFAULT 'pending',"
        "  operation_id VARCHAR(64) DEFAULT '',"
        "  payment_type VARCHAR(16) DEFAULT '',"
        "  created_at TIMESTAMPTZ DEFAULT now(),"
        "  paid_at TIMESTAMPTZ"
        ")");
    if (!premiumPaymentsTable) {
        Logger::getInstance().warning("Could not ensure premium_payments table exists: " + db_->getLastError());
    } else {
        PQclear(premiumPaymentsTable);
    }
    PGresult* premiumPaymentsGiftCol = db_->executeQuery(
        "ALTER TABLE IF EXISTS premium_payments "
        "ADD COLUMN IF NOT EXISTS gift_receiver_id UUID REFERENCES users(id) ON DELETE SET NULL");
    if (!premiumPaymentsGiftCol) {
        Logger::getInstance().warning("Could not ensure premium_payments.gift_receiver_id exists: " + db_->getLastError());
    } else {
        PQclear(premiumPaymentsGiftCol);
    }
    PGresult* premiumPaymentsIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_premium_payments_user_id ON premium_payments(user_id)");
    if (premiumPaymentsIdx) {
        PQclear(premiumPaymentsIdx);
    }

    // Users: admin fields (non-destructive).
    PGresult* usersAdminCols = db_->executeQuery(
        "ALTER TABLE users "
        "ADD COLUMN IF NOT EXISTS is_admin BOOLEAN DEFAULT FALSE, "
        "ADD COLUMN IF NOT EXISTS role VARCHAR(32) DEFAULT 'user', "
        "ADD COLUMN IF NOT EXISTS totp_secret TEXT DEFAULT '', "
        "ADD COLUMN IF NOT EXISTS admin_ip_whitelist TEXT DEFAULT ''");
    if (!usersAdminCols) {
        Logger::getInstance().warning("Could not ensure users admin columns exist: " + db_->getLastError());
    } else {
        PQclear(usersAdminCols);
    }

    // Admin settings (key/value)
    PGresult* adminSettingsTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS admin_settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL,"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()"
        ")");
    if (!adminSettingsTable) {
        Logger::getInstance().warning("Could not ensure admin_settings table exists: " + db_->getLastError());
    } else {
        PQclear(adminSettingsTable);
    }

    // Backfill bot_user_id for legacy Bot Builder bots (created before bot_user_id existed).
    // We avoid a PL/pgSQL DO block here to prevent subtle quoting/syntax issues across deployments.
    // 1) Link to existing bot users by username
    PGresult* botBuilderBackfillLink1 = db_->executeQuery(
        "UPDATE bot_builder_bots b "
        "SET bot_user_id = u.id "
        "FROM users u "
        "WHERE b.bot_user_id IS NULL "
        "  AND COALESCE(b.bot_username,'') <> '' "
        "  AND lower(u.username) = lower(b.bot_username) "
        "  AND COALESCE(u.is_bot, FALSE) = TRUE");
    if (!botBuilderBackfillLink1) {
        Logger::getInstance().warning("Could not link existing bot users for bot_builder_bots.bot_user_id: " + db_->getLastError());
    } else {
        PQclear(botBuilderBackfillLink1);
    }

    // 2) Create missing bot users (only when username is completely free in users table)
    PGresult* botBuilderBackfillInsert = db_->executeQuery(
        "WITH missing AS ("
        "  SELECT DISTINCT b.bot_username "
        "  FROM bot_builder_bots b "
        "  LEFT JOIN users u ON lower(u.username) = lower(b.bot_username) "
        "  WHERE b.bot_user_id IS NULL "
        "    AND COALESCE(b.bot_username,'') <> '' "
        "    AND u.id IS NULL"
        ") "
        "INSERT INTO users (username, password_hash, is_active, is_bot) "
        "SELECT m.bot_username, 'migrated_bot_' || md5(random()::text), TRUE, TRUE "
        "FROM missing m");
    if (!botBuilderBackfillInsert) {
        Logger::getInstance().warning("Could not create missing bot users for bot_builder_bots.bot_user_id: " + db_->getLastError());
    } else {
        PQclear(botBuilderBackfillInsert);
    }

    // 3) Link again (for newly inserted users)
    PGresult* botBuilderBackfillLink2 = db_->executeQuery(
        "UPDATE bot_builder_bots b "
        "SET bot_user_id = u.id "
        "FROM users u "
        "WHERE b.bot_user_id IS NULL "
        "  AND COALESCE(b.bot_username,'') <> '' "
        "  AND lower(u.username) = lower(b.bot_username) "
        "  AND COALESCE(u.is_bot, FALSE) = TRUE");
    if (!botBuilderBackfillLink2) {
        Logger::getInstance().warning("Could not finalize bot_user_id backfill for bot_builder_bots: " + db_->getLastError());
    } else {
        PQclear(botBuilderBackfillLink2);
    }

    // Make legacy bots appear in chats list: ensure owner<->bot friendship exists.
    // This is idempotent and safe due to ON CONFLICT DO NOTHING.
    PGresult* botBuilderFriendBackfill = db_->executeQuery(
        "INSERT INTO friends (user1_id, user2_id) "
        "SELECT LEAST(user_id, bot_user_id), GREATEST(user_id, bot_user_id) "
        "FROM bot_builder_bots "
        "WHERE bot_user_id IS NOT NULL "
        "ON CONFLICT (user1_id, user2_id) DO NOTHING");
    if (!botBuilderFriendBackfill) {
        Logger::getInstance().warning("Could not backfill friendships for legacy bots: " + db_->getLastError());
    } else {
        PQclear(botBuilderFriendBackfill);
    }

    PGresult* pushTokensTable = db_->executeQuery(
        "CREATE TABLE IF NOT EXISTS user_push_tokens ("
        "  user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,"
        "  device_token TEXT NOT NULL,"
        "  platform VARCHAR(16) NOT NULL DEFAULT 'android',"
        "  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "  updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "  PRIMARY KEY (user_id, device_token)"
        ")");
    if (!pushTokensTable) {
        Logger::getInstance().warning("Could not ensure user_push_tokens table exists: " + db_->getLastError());
    } else {
        PQclear(pushTokensTable);
    }
    PGresult* pushTokensIdx = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_user_push_tokens_user "
        "ON user_push_tokens (user_id)");
    if (pushTokensIdx) PQclear(pushTokensIdx);

    PGresult* sessionUserAgentCol = db_->executeQuery(
        "ALTER TABLE IF EXISTS user_sessions "
        "ADD COLUMN IF NOT EXISTS user_agent TEXT");
    if (!sessionUserAgentCol) {
        Logger::getInstance().warning("Could not ensure user_sessions.user_agent exists: " + db_->getLastError());
    } else {
        PQclear(sessionUserAgentCol);
    }

    // Public Directory: Add category and is_public columns to groups
    PGresult* groupsCategoryCol = db_->executeQuery(
        "ALTER TABLE groups ADD COLUMN IF NOT EXISTS category VARCHAR(50) DEFAULT ''");
    if (groupsCategoryCol) PQclear(groupsCategoryCol);
    
    PGresult* groupsIsPublicCol = db_->executeQuery(
        "ALTER TABLE groups ADD COLUMN IF NOT EXISTS is_public BOOLEAN DEFAULT FALSE");
    if (groupsIsPublicCol) PQclear(groupsIsPublicCol);
    
    // Public Directory: Add category and is_verified columns to channels
    PGresult* channelsCategoryCol = db_->executeQuery(
        "ALTER TABLE channels ADD COLUMN IF NOT EXISTS category VARCHAR(50) DEFAULT ''");
    if (channelsCategoryCol) PQclear(channelsCategoryCol);
    
    PGresult* channelsIsVerifiedCol = db_->executeQuery(
        "ALTER TABLE channels ADD COLUMN IF NOT EXISTS is_verified BOOLEAN DEFAULT FALSE");
    if (channelsIsVerifiedCol) PQclear(channelsIsVerifiedCol);
    
    // Indexes for public directory queries
    PGresult* idxGroupsPublic = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_groups_is_public ON groups(is_public) WHERE is_public = TRUE");
    if (idxGroupsPublic) PQclear(idxGroupsPublic);
    
    PGresult* idxChannelsPrivate = db_->executeQuery(
        "CREATE INDEX IF NOT EXISTS idx_channels_is_private_false ON channels(is_private) WHERE is_private = FALSE");
    if (idxChannelsPrivate) PQclear(idxChannelsPrivate);
    
    prepareStatements();
    Logger::getInstance().info("Database initialized successfully");
    return true;
}

void DatabaseManager::prepareStatements() {
    // Prepare statements for better performance and security
    db_->prepareStatement("get_user_by_username",
        "SELECT id, username, password_hash, created_at, is_active, "
        "COALESCE(is_admin, FALSE), COALESCE(role, 'user'), "
        "COALESCE(totp_secret, ''), COALESCE(admin_ip_whitelist, ''), COALESCE(is_bot, FALSE), "
        "CASE WHEN COALESCE(is_premium, FALSE) AND (premium_expires_at IS NULL OR premium_expires_at > now()) "
        "THEN TRUE ELSE FALSE END, "
        "COALESCE(premium_plan, ''), "
        "COALESCE(premium_expires_at::text, '') "
        "FROM users WHERE username = $1");
    
    db_->prepareStatement("get_user_by_id",
        "SELECT id, username, password_hash, created_at, is_active, "
        "COALESCE(is_admin, FALSE), COALESCE(role, 'user'), "
        "COALESCE(totp_secret, ''), COALESCE(admin_ip_whitelist, ''), COALESCE(is_bot, FALSE), "
        "CASE WHEN COALESCE(is_premium, FALSE) AND (premium_expires_at IS NULL OR premium_expires_at > now()) "
        "THEN TRUE ELSE FALSE END, "
        "COALESCE(premium_plan, ''), "
        "COALESCE(premium_expires_at::text, '') "
        "FROM users WHERE id = $1");

    // Lightweight public profile lookup
    db_->prepareStatement("get_user_public",
        "SELECT id::text, username, COALESCE(avatar_url, '') "
        "FROM users WHERE id = $1::uuid");
    
    db_->prepareStatement("check_username_exists",
        "SELECT COUNT(*) FROM users WHERE username = $1");
    
    db_->prepareStatement("are_friends",
        "SELECT COUNT(*) FROM friends WHERE (user1_id = $1 AND user2_id = $2) OR (user1_id = $2 AND user2_id = $1)");
    
    // Prepared statements prevent SQL injection for auth flows.
    db_->prepareStatement("create_user",
        "INSERT INTO users (username, password_hash) VALUES ($1, $2)");

    db_->prepareStatement("create_bot_user",
        "INSERT INTO users (username, password_hash, is_active, is_bot) "
        "VALUES ($1, $2, TRUE, TRUE) RETURNING id");
    
    db_->prepareStatement("update_last_login",
        "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = $1");
    
    db_->prepareStatement("update_last_activity",
        "UPDATE users SET last_activity = CURRENT_TIMESTAMP WHERE id = $1");

    // Persistent sessions (survive restarts)
    db_->prepareStatement("upsert_user_session",
        "INSERT INTO user_sessions (token, user_id, expires_at) "
        "VALUES ($1, $2::uuid, now() + ($3::text || ' seconds')::interval) "
        "ON CONFLICT (token) DO UPDATE SET user_id = EXCLUDED.user_id, expires_at = EXCLUDED.expires_at, last_seen = now()");
    db_->prepareStatement("get_user_id_by_session",
        "SELECT user_id FROM user_sessions WHERE token = $1 AND (expires_at IS NULL OR expires_at > now())");
    db_->prepareStatement("touch_user_session",
        "UPDATE user_sessions SET last_seen = now() WHERE token = $1");
    db_->prepareStatement("delete_user_session",
        "DELETE FROM user_sessions WHERE token = $1");
    db_->prepareStatement("list_user_sessions",
        "SELECT token, COALESCE(created_at::text, ''), COALESCE(last_seen::text, ''), "
        "COALESCE(user_agent, '') "
        "FROM user_sessions "
        "WHERE user_id = $1::uuid AND (expires_at IS NULL OR expires_at > now()) "
        "ORDER BY last_seen DESC");
    db_->prepareStatement("update_user_session_user_agent",
        "UPDATE user_sessions SET user_agent = $2 WHERE token = $1");

    db_->prepareStatement("upsert_push_token",
        "INSERT INTO user_push_tokens (user_id, device_token, platform) "
        "VALUES ($1::uuid, $2, $3) "
        "ON CONFLICT (user_id, device_token) DO UPDATE "
        "SET platform = EXCLUDED.platform, updated_at = now()");
    db_->prepareStatement("delete_push_token",
        "DELETE FROM user_push_tokens WHERE user_id = $1::uuid AND device_token = $2");
    db_->prepareStatement("get_push_tokens",
        "SELECT device_token, platform FROM user_push_tokens WHERE user_id = $1::uuid");
    
    db_->prepareStatement("set_user_active",
        "UPDATE users SET is_active = $2 WHERE id = $1");
    
    db_->prepareStatement("update_user_password_hash",
        "UPDATE users SET password_hash = $2 WHERE id = $1");

    db_->prepareStatement("set_user_role",
        "UPDATE users SET role = $2 WHERE id = $1::uuid");

    db_->prepareStatement("set_user_admin_flag",
        "UPDATE users SET is_admin = $2 WHERE id = $1::uuid");

    db_->prepareStatement("delete_user_sessions_by_user",
        "DELETE FROM user_sessions WHERE user_id = $1::uuid");

    db_->prepareStatement("count_banned_users",
        "SELECT COUNT(*) FROM users WHERE is_active = FALSE");

    db_->prepareStatement("count_active_users_since",
        "SELECT COUNT(*) FROM users WHERE last_activity IS NOT NULL "
        "AND last_activity > (now() - ($1::text || ' seconds')::interval)");

    db_->prepareStatement("count_groups",
        "SELECT COUNT(*) FROM groups");

    db_->prepareStatement("count_channels",
        "SELECT COUNT(*) FROM channels");

    db_->prepareStatement("count_bots",
        "SELECT COUNT(*) FROM bot_builder_bots");

    db_->prepareStatement("count_reports_by_status",
        "SELECT COUNT(*) FROM message_reports WHERE ($1 = '' OR status = $1)");

    db_->prepareStatement("upsert_admin_setting",
        "INSERT INTO admin_settings (key, value) VALUES ($1, $2) "
        "ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value, updated_at = now()");

    db_->prepareStatement("get_admin_setting",
        "SELECT value FROM admin_settings WHERE key = $1");
    
    db_->prepareStatement("delete_messages_by_user",
        "DELETE FROM messages WHERE sender_id = $1 OR receiver_id = $1");
    
    db_->prepareStatement("count_users",
        "SELECT COUNT(*) FROM users");
    
    db_->prepareStatement("count_messages_today",
        "SELECT COUNT(*) FROM messages WHERE created_at::date = CURRENT_DATE");
    
    db_->prepareStatement("count_online_users",
        "SELECT COUNT(*) FROM users WHERE last_activity IS NOT NULL "
        "AND (EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - last_activity))::int < $1)");

    db_->prepareStatement("admin_search_users",
        "SELECT id::text, username, COALESCE(is_active, TRUE), COALESCE(is_admin, FALSE), "
        "COALESCE(is_bot, FALSE), COALESCE(role, 'user'), COALESCE(last_login::text,''), "
        "COALESCE(last_activity::text,''), created_at::text "
        "FROM users "
        "WHERE username ILIKE $1 OR id::text = $2 "
        "ORDER BY CASE "
        "WHEN id::text = $2 THEN 0 "
        "WHEN lower(username) = lower($3) THEN 1 "
        "WHEN lower(username) LIKE lower($3) || '%' THEN 2 "
        "ELSE 3 END, username "
        "LIMIT $4");

    db_->prepareStatement("admin_search_messages",
        "SELECT m.id::text, m.sender_id::text, COALESCE(s.username,''), "
        "m.receiver_id::text, COALESCE(r.username,''), m.content, "
        "COALESCE(m.message_type,'text'), m.created_at::text "
        "FROM messages m "
        "LEFT JOIN users s ON s.id = m.sender_id "
        "LEFT JOIN users r ON r.id = m.receiver_id "
        "WHERE m.content ILIKE $1 OR m.id::text = $2 "
        "ORDER BY m.created_at DESC "
        "LIMIT $3");

    db_->prepareStatement("admin_search_groups",
        "SELECT id::text, name, COALESCE(description,''), creator_id::text, "
        "COALESCE(invite_link,''), created_at::text "
        "FROM groups "
        "WHERE name ILIKE $1 OR id::text = $2 "
        "ORDER BY name "
        "LIMIT $3");

    db_->prepareStatement("admin_search_channels",
        "SELECT id::text, name, COALESCE(description,''), creator_id::text, "
        "COALESCE(custom_link,''), created_at::text "
        "FROM channels "
        "WHERE name ILIKE $1 OR custom_link ILIKE $1 OR id::text = $2 "
        "ORDER BY name "
        "LIMIT $3");
    
    db_->prepareStatement("update_user_avatar",
        "UPDATE users SET avatar_url = $1 WHERE id = $2");

    db_->prepareStatement("set_user_premium",
        "UPDATE users SET is_premium = $2::boolean, premium_plan = NULLIF($3, ''), "
        "premium_expires_at = CASE "
        "WHEN $2::boolean THEN "
        "CASE "
        "WHEN $3 = 'year' THEN "
        "  (CASE WHEN premium_expires_at IS NOT NULL AND premium_expires_at > now() "
        "        THEN premium_expires_at ELSE now() END) + interval '1 year' "
        "WHEN $3 = 'half' THEN "
        "  (CASE WHEN premium_expires_at IS NOT NULL AND premium_expires_at > now() "
        "        THEN premium_expires_at ELSE now() END) + interval '6 months' "
        "WHEN $3 = 'month' THEN "
        "  (CASE WHEN premium_expires_at IS NOT NULL AND premium_expires_at > now() "
        "        THEN premium_expires_at ELSE now() END) + interval '1 month' "
        "WHEN $3 = 'trial' THEN "
        "  (CASE WHEN premium_expires_at IS NOT NULL AND premium_expires_at > now() "
        "        THEN premium_expires_at ELSE now() END) + interval '7 days' "
        "ELSE NULL "
        "END "
        "ELSE NULL "
        "END "
        "WHERE id = $1::uuid");

    db_->prepareStatement("create_premium_payment",
        "INSERT INTO premium_payments (user_id, plan, amount, label) "
        "VALUES ($1::uuid, $2, $3::numeric, 'pp_' || uuid_generate_v4()) "
        "RETURNING id::text, label, plan, amount::text, status");

    db_->prepareStatement("create_premium_gift_payment",
        "INSERT INTO premium_payments (user_id, plan, amount, label, gift_receiver_id) "
        "VALUES ($1::uuid, $2, $3::numeric, 'pp_' || uuid_generate_v4(), $4::uuid) "
        "RETURNING id::text, label, plan, amount::text, status, COALESCE(gift_receiver_id::text, '')");

    db_->prepareStatement("get_premium_payment_by_label",
        "SELECT id::text, user_id::text, plan, amount::text, status, "
        "COALESCE(operation_id, ''), COALESCE(payment_type, ''), "
        "COALESCE(gift_receiver_id::text, ''), "
        "created_at::text, COALESCE(paid_at::text, '') "
        "FROM premium_payments WHERE label = $1");

    db_->prepareStatement("mark_premium_payment_paid",
        "UPDATE premium_payments "
        "SET status = 'paid', operation_id = $2, payment_type = $3, paid_at = now() "
        "WHERE label = $1 AND status <> 'paid' "
        "RETURNING id");

    db_->prepareStatement("reset_premium_payment_status",
        "UPDATE premium_payments "
        "SET status = 'pending', operation_id = '', payment_type = '', paid_at = NULL "
        "WHERE label = $1 AND status = 'paid' "
        "RETURNING id");

    db_->prepareStatement("has_trial_payment",
        "SELECT 1 FROM premium_payments WHERE user_id = $1::uuid AND plan = 'trial' AND status = 'paid' LIMIT 1");

    db_->prepareStatement("admin_list_premium_payments",
        "SELECT p.id::text, p.label, p.plan, p.amount::text, p.status, "
        "COALESCE(p.operation_id, ''), COALESCE(p.payment_type, ''), "
        "p.created_at::text, COALESCE(p.paid_at::text, ''), "
        "u.id::text, u.username "
        "FROM premium_payments p "
        "JOIN users u ON u.id = p.user_id "
        "WHERE ($1 = '' OR p.status = $1) "
        "ORDER BY p.created_at DESC "
        "LIMIT $2");
    
    db_->prepareStatement("get_user_last_activity",
        "SELECT last_activity FROM users WHERE id = $1");
    
    db_->prepareStatement("check_user_online",
        "SELECT CASE WHEN last_activity IS NOT NULL AND (EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - last_activity))::int < $2) THEN true ELSE false END as is_online FROM users WHERE id = $1");

    // Profile + privacy
    db_->prepareStatement("get_user_profile",
        "SELECT COALESCE(first_name,''), COALESCE(last_name,''), COALESCE(bio,''), "
        "COALESCE(birth_day,0), COALESCE(birth_month,0), COALESCE(birth_year,0), "
        "COALESCE(linked_channel_id::text, ''), COALESCE(business_hours_json::text, '') "
        "FROM user_profiles WHERE user_id = $1::uuid");
    db_->prepareStatement("upsert_user_profile",
        "INSERT INTO user_profiles (user_id, first_name, last_name, bio, birth_day, birth_month, birth_year) "
        "VALUES ($1::uuid, $2, $3, $4, NULLIF($5::int,0), NULLIF($6::int,0), NULLIF($7::int,0)) "
        "ON CONFLICT (user_id) DO UPDATE SET first_name = EXCLUDED.first_name, last_name = EXCLUDED.last_name, "
        "bio = EXCLUDED.bio, birth_day = EXCLUDED.birth_day, birth_month = EXCLUDED.birth_month, birth_year = EXCLUDED.birth_year");
    db_->prepareStatement("upsert_user_linked_channel",
        "INSERT INTO user_profiles (user_id, linked_channel_id) "
        "VALUES ($1::uuid, NULLIF($2,'')::uuid) "
        "ON CONFLICT (user_id) DO UPDATE SET linked_channel_id = EXCLUDED.linked_channel_id");
    db_->prepareStatement("set_user_business_hours",
        "INSERT INTO user_profiles (user_id, business_hours_json) "
        "VALUES ($1::uuid, NULLIF($2,'')::jsonb) "
        "ON CONFLICT (user_id) DO UPDATE SET business_hours_json = EXCLUDED.business_hours_json");
    db_->prepareStatement("get_user_privacy",
        "SELECT bio_visibility, birth_visibility, last_seen_visibility, avatar_visibility, send_read_receipts "
        "FROM user_privacy_settings WHERE user_id = $1::uuid");
    db_->prepareStatement("upsert_user_privacy",
        "INSERT INTO user_privacy_settings (user_id, bio_visibility, birth_visibility, last_seen_visibility, avatar_visibility, send_read_receipts) "
        "VALUES ($1::uuid, $2, $3, $4, $5, $6) "
        "ON CONFLICT (user_id) DO UPDATE SET bio_visibility = EXCLUDED.bio_visibility, "
        "birth_visibility = EXCLUDED.birth_visibility, last_seen_visibility = EXCLUDED.last_seen_visibility, "
        "avatar_visibility = EXCLUDED.avatar_visibility, send_read_receipts = EXCLUDED.send_read_receipts");

    db_->prepareStatement("get_chat_pins",
        "SELECT chat_type, chat_id FROM user_chat_pins WHERE user_id = $1::uuid ORDER BY created_at ASC");
    db_->prepareStatement("pin_chat",
        "INSERT INTO user_chat_pins (user_id, chat_type, chat_id) VALUES ($1::uuid, $2, $3::uuid) "
        "ON CONFLICT (user_id, chat_type, chat_id) DO NOTHING");
    db_->prepareStatement("unpin_chat",
        "DELETE FROM user_chat_pins WHERE user_id = $1::uuid AND chat_type = $2 AND chat_id = $3::uuid");

    db_->prepareStatement("get_chat_folders",
        "SELECT COALESCE(folders::text, '[]') FROM user_chat_folders WHERE user_id = $1::uuid");
    db_->prepareStatement("set_chat_folders",
        "INSERT INTO user_chat_folders (user_id, folders) VALUES ($1::uuid, $2::jsonb) "
        "ON CONFLICT (user_id) DO UPDATE SET folders = EXCLUDED.folders, updated_at = now()");

    db_->prepareStatement("count_user_channels_v2",
        "SELECT COUNT(*) FROM chats WHERE type = 'channel' AND created_by = $1::uuid");
    db_->prepareStatement("count_user_channels_legacy",
        "SELECT COUNT(*) FROM channels WHERE creator_id = $1");
    db_->prepareStatement("count_user_public_links_v2",
        "SELECT COUNT(*) FROM chats WHERE type = 'channel' AND created_by = $1::uuid "
        "AND COALESCE(username, '') <> ''");
    db_->prepareStatement("count_user_public_links_legacy",
        "SELECT COUNT(*) FROM channels WHERE creator_id = $1 AND COALESCE(custom_link, '') <> ''");
    
    db_->prepareStatement("create_friend_request",
        "INSERT INTO friend_requests (sender_id, receiver_id) VALUES ($1, $2)");
    
    db_->prepareStatement("get_friend_request",
        "SELECT id, sender_id, receiver_id FROM friend_requests WHERE id = $1");
    
    db_->prepareStatement("update_friend_request_status",
        "UPDATE friend_requests SET status = $1 WHERE id = $2");
    
    db_->prepareStatement("create_friendship",
        "INSERT INTO friends (user1_id, user2_id) VALUES ($1, $2)");
    db_->prepareStatement("create_friendship_pair",
        "INSERT INTO friends (user1_id, user2_id) VALUES (LEAST($1::uuid, $2::uuid), GREATEST($1::uuid, $2::uuid)) "
        "ON CONFLICT (user1_id, user2_id) DO NOTHING");
    
    db_->prepareStatement("get_friend_requests",
        "SELECT id, sender_id, receiver_id, status, created_at FROM friend_requests WHERE receiver_id = $1 AND status = 'pending'");
    
    db_->prepareStatement("get_friends",
        "SELECT u.id, u.username, COALESCE(u.avatar_url, '') as avatar_url, f.created_at FROM friends f "
        "JOIN users u ON (u.id = CASE WHEN f.user1_id = $1 THEN f.user2_id ELSE f.user1_id END) "
        "WHERE f.user1_id = $1 OR f.user2_id = $1");
    
    db_->prepareStatement("send_message",
        "INSERT INTO messages (sender_id, receiver_id, content, message_type, file_path, file_name, file_size, reply_to_message_id, reply_markup) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, CASE WHEN $9 = '' THEN NULL ELSE $9::jsonb END)");
    Logger::getInstance().info("Prepared statement 'send_message' creation attempted");
    
    db_->prepareStatement("get_messages",
        "SELECT m.id, m.sender_id, m.receiver_id, m.content, m.created_at, m.is_read, m.is_delivered, m.message_type, "
        "m.file_path, m.file_name, m.file_size, m.reply_to_message_id, "
        "COALESCE(m.reply_markup::text, '') as reply_markup, "
        "COALESCE(dp.message_id = m.id, FALSE) AS is_pinned "
        "FROM messages m "
        "LEFT JOIN direct_pins dp ON dp.user_low = LEAST($1::uuid, $2::uuid) AND dp.user_high = GREATEST($1::uuid, $2::uuid) "
        "WHERE (m.sender_id = $1 AND m.receiver_id = $2) OR (m.sender_id = $2 AND m.receiver_id = $1) "
        "ORDER BY m.created_at DESC LIMIT $3");
    
    // Special query for saved messages (when user_id == friend_id)
    db_->prepareStatement("get_saved_messages",
        "SELECT id, sender_id, receiver_id, content, created_at, is_read, is_delivered, message_type, file_path, file_name, file_size, reply_to_message_id, "
        "COALESCE(reply_markup::text, '') as reply_markup, "
        "FALSE AS is_pinned "
        "FROM messages "
        "WHERE sender_id = $1 AND receiver_id = $1 "
        "ORDER BY created_at DESC LIMIT $2");
    
    db_->prepareStatement("get_last_message",
        "SELECT id, sender_id, receiver_id, content, created_at, is_read, is_delivered, message_type, file_path, file_name, file_size, reply_to_message_id, "
        "COALESCE(reply_markup::text, '') as reply_markup "
        "FROM messages "
        "WHERE (sender_id = $1 AND receiver_id = $2) OR (sender_id = $2 AND receiver_id = $1) "
        "ORDER BY created_at DESC LIMIT 1");
    
    // Special query for saved messages last message
    db_->prepareStatement("get_last_saved_message",
        "SELECT id, sender_id, receiver_id, content, created_at, is_read, is_delivered, message_type, file_path, file_name, file_size, reply_to_message_id, "
        "COALESCE(reply_markup::text, '') as reply_markup "
        "FROM messages "
        "WHERE sender_id = $1 AND receiver_id = $1 "
        "ORDER BY created_at DESC LIMIT 1");
    
    db_->prepareStatement("get_message_by_id",
        "SELECT id, sender_id, receiver_id, content, created_at, is_read, is_delivered, message_type, file_path, file_name, file_size, reply_to_message_id, "
        "COALESCE(reply_markup::text, '') as reply_markup "
        "FROM messages WHERE id = $1");

    db_->prepareStatement("delete_message_by_id",
        "DELETE FROM messages WHERE id = $1");
    
    db_->prepareStatement("get_unread_count",
        "SELECT COUNT(*) FROM messages WHERE receiver_id = $1 AND sender_id = $2 AND is_read = false");
    
    db_->prepareStatement("mark_messages_read",
        "UPDATE messages SET is_read = true WHERE receiver_id = $1 AND sender_id = $2 AND is_read = false");

    db_->prepareStatement("mark_messages_delivered",
        "UPDATE messages SET is_delivered = true WHERE receiver_id = $1 AND sender_id = $2 AND is_delivered = false");

    db_->prepareStatement("mark_message_delivered_by_id",
        "UPDATE messages SET is_delivered = true WHERE id = $1::uuid AND is_delivered = false");
    
    db_->prepareStatement("get_chat_partners",
        "SELECT u.id, u.username, COALESCE(u.avatar_url, '') as avatar_url, COALESCE(MAX(m.created_at)::text, '') as last_message_time "
        "FROM messages m "
        "JOIN users u ON (u.id = CASE WHEN m.sender_id = $1 THEN m.receiver_id ELSE m.sender_id END) "
        "WHERE (m.sender_id = $1 OR m.receiver_id = $1) "
        "GROUP BY u.id, u.username, u.avatar_url "
        "ORDER BY MAX(m.created_at) DESC NULLS LAST");
    
    // Check if user has saved messages
    db_->prepareStatement("has_saved_messages",
        "SELECT COUNT(*) > 0 FROM messages WHERE sender_id = $1 AND receiver_id = $1");
    db_->prepareStatement("pin_direct_message",
        "INSERT INTO direct_pins (user_low, user_high, message_id, pinned_by) "
        "VALUES (LEAST($1::uuid, $2::uuid), GREATEST($1::uuid, $2::uuid), $3::uuid, $4::uuid) "
        "ON CONFLICT (user_low, user_high) DO UPDATE SET message_id = EXCLUDED.message_id, pinned_by = EXCLUDED.pinned_by, pinned_at = now()");
    db_->prepareStatement("unpin_direct_message",
        "DELETE FROM direct_pins WHERE user_low = LEAST($1::uuid, $2::uuid) AND user_high = GREATEST($1::uuid, $2::uuid)");
    db_->prepareStatement("get_direct_pinned_message",
        "SELECT message_id FROM direct_pins WHERE user_low = LEAST($1::uuid, $2::uuid) AND user_high = GREATEST($1::uuid, $2::uuid) LIMIT 1");
    
    // Group statements
    db_->prepareStatement("create_group",
        "INSERT INTO groups (name, description, creator_id) VALUES ($1, $2, $3) RETURNING id");
    db_->prepareStatement("get_group_by_id",
        "SELECT id, name, description, creator_id, invite_link, created_at FROM groups WHERE id = $1");
    db_->prepareStatement("get_user_groups",
        "SELECT g.id, g.name, g.description, g.creator_id, g.invite_link, g.created_at "
        "FROM groups g JOIN group_members gm ON g.id = gm.group_id "
        "WHERE gm.user_id = $1 AND gm.is_banned = FALSE ORDER BY g.created_at DESC");
    db_->prepareStatement("add_group_member",
        "INSERT INTO group_members (group_id, user_id, role) VALUES ($1, $2, $3) "
        "ON CONFLICT (group_id, user_id) DO UPDATE SET is_banned = FALSE, role = $3");
    db_->prepareStatement("send_group_message",
        "INSERT INTO group_messages (group_id, sender_id, content, message_type, file_path, file_name, file_size, "
        "reply_to_message_id, forwarded_from_user_id, forwarded_from_username, forwarded_from_message_id) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)");
    db_->prepareStatement("get_group_messages",
        "SELECT gm.id, gm.group_id, gm.sender_id, u.username, gm.content, gm.message_type, "
        "gm.file_path, gm.file_name, gm.file_size, gm.reply_to_message_id, "
        "gm.forwarded_from_user_id, gm.forwarded_from_username, gm.forwarded_from_message_id, "
        "gm.is_pinned, gm.created_at "
        "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
        "WHERE gm.group_id = $1 AND gm.topic_id IS NULL ORDER BY gm.created_at DESC LIMIT $2");
    db_->prepareStatement("create_group_invite",
        "INSERT INTO group_invites (group_id, invite_link, created_by, expires_at) VALUES ($1, $2, $3, $4)");
    db_->prepareStatement("update_group_name",
        "UPDATE groups SET name = $1 WHERE id = $2");
    db_->prepareStatement("update_group_description",
        "UPDATE groups SET description = $1 WHERE id = $2");
    db_->prepareStatement("pin_group_message",
        "INSERT INTO pinned_messages (group_id, message_id, pinned_by) VALUES ($1, $2, $3) "
        "ON CONFLICT (group_id, message_id) DO NOTHING");
    db_->prepareStatement("update_message_pinned",
        "UPDATE group_messages SET is_pinned = TRUE WHERE id = $1");
    db_->prepareStatement("remove_group_member",
        "DELETE FROM group_members WHERE group_id = $1 AND user_id = $2");
    db_->prepareStatement("update_group_member_role",
        "UPDATE group_members SET role = $1 WHERE group_id = $2 AND user_id = $3");
    db_->prepareStatement("mute_group_member",
        "UPDATE group_members SET is_muted = $1 WHERE group_id = $2 AND user_id = $3");
    db_->prepareStatement("ban_group_member",
        "UPDATE group_members SET is_banned = $1, banned_until = $2 WHERE group_id = $3 AND user_id = $4");
    db_->prepareStatement("get_group_members",
        "SELECT gm.id, gm.group_id, gm.user_id, u.username, gm.role, gm.is_muted, gm.is_banned, gm.joined_at, COALESCE(gm.permissions::text, '{}') "
        "FROM group_members gm JOIN users u ON gm.user_id = u.id WHERE gm.group_id = $1");
    db_->prepareStatement("get_group_member",
        "SELECT gm.id, gm.group_id, gm.user_id, u.username, gm.role, gm.is_muted, gm.is_banned, gm.joined_at, COALESCE(gm.permissions::text, '{}') "
        "FROM group_members gm JOIN users u ON gm.user_id = u.id WHERE gm.group_id = $1 AND gm.user_id = $2");
    db_->prepareStatement("update_admin_permissions",
        "UPDATE group_members SET permissions = $1::jsonb WHERE group_id = $2 AND user_id = $3");
    db_->prepareStatement("get_invite_info",
        "SELECT group_id, expires_at, max_uses, current_uses FROM group_invites WHERE invite_link = $1");
    db_->prepareStatement("unpin_group_message",
        "DELETE FROM pinned_messages WHERE group_id = $1 AND message_id = $2");
    db_->prepareStatement("update_message_unpinned",
        "UPDATE group_messages SET is_pinned = FALSE WHERE id = $1");
    
    // Group management statements
    db_->prepareStatement("delete_group",
        "DELETE FROM groups WHERE id = $1");
    db_->prepareStatement("delete_group_messages",
        "DELETE FROM group_messages WHERE group_id = $1");
    db_->prepareStatement("delete_group_members",
        "DELETE FROM group_members WHERE group_id = $1");
    db_->prepareStatement("delete_group_invites",
        "DELETE FROM group_invites WHERE group_id = $1");
    db_->prepareStatement("delete_group_pinned",
        "DELETE FROM pinned_messages WHERE group_id = $1");
    db_->prepareStatement("search_group_messages",
        "SELECT gm.id, gm.group_id, gm.sender_id, u.username, gm.content, gm.message_type, "
        "COALESCE(gm.file_path, ''), COALESCE(gm.file_name, ''), COALESCE(gm.file_size, 0), "
        "gm.created_at, COALESCE(gm.is_pinned, false) "
        "FROM group_messages gm JOIN users u ON gm.sender_id = u.id "
        "WHERE gm.group_id = $1 AND gm.content ILIKE $2 ORDER BY gm.created_at DESC LIMIT $3");
    db_->prepareStatement("update_group_permissions",
        "UPDATE groups SET permissions = COALESCE(permissions, '{}'::jsonb) || $1::jsonb WHERE id = $2");
    
    // Channel statements
    db_->prepareStatement("create_channel",
        "INSERT INTO channels (name, description, creator_id, custom_link) VALUES ($1, $2, $3, $4) RETURNING id");
    db_->prepareStatement("check_channel_custom_link",
        "SELECT COUNT(*) FROM channels WHERE custom_link = $1");
    db_->prepareStatement("get_channel_by_id",
        "SELECT id, name, description, creator_id, custom_link, COALESCE(avatar_url, ''), is_private, show_author, created_at, COALESCE(is_verified, false) FROM channels WHERE id = $1");
    db_->prepareStatement("get_channel_by_custom_link",
        "SELECT id, name, description, creator_id, custom_link, COALESCE(avatar_url, ''), is_private, show_author, created_at, COALESCE(is_verified, false) FROM channels WHERE custom_link = $1");
    db_->prepareStatement("get_user_channels",
        "SELECT c.id, c.name, c.description, c.creator_id, c.custom_link, COALESCE(c.avatar_url, ''), c.is_private, c.show_author, c.created_at, COALESCE(c.is_verified, false) "
        "FROM channels c JOIN channel_members cm ON c.id = cm.channel_id "
        "WHERE cm.user_id = $1 AND cm.is_banned = FALSE ORDER BY c.created_at DESC");
    db_->prepareStatement("add_channel_member",
        "INSERT INTO channel_members (channel_id, user_id, role) VALUES ($1, $2, $3) "
        "ON CONFLICT (channel_id, user_id) DO UPDATE SET is_banned = FALSE, role = $3");
    db_->prepareStatement("send_channel_message",
        "INSERT INTO channel_messages (channel_id, sender_id, content, message_type, file_path, file_name, file_size) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7)");
    db_->prepareStatement("get_channel_messages",
        "SELECT cm.id, cm.channel_id, cm.sender_id, u.username, cm.content, cm.message_type, "
        "cm.file_path, cm.file_name, cm.file_size, cm.is_pinned, "
        "COALESCE((SELECT COUNT(*) FROM channel_message_views WHERE message_id = cm.id), 0) as views_count, "
        "cm.created_at "
        "FROM channel_messages cm JOIN users u ON cm.sender_id = u.id "
        "WHERE cm.channel_id = $1 ORDER BY cm.created_at DESC LIMIT $2");
    db_->prepareStatement("add_message_reaction",
        "INSERT INTO channel_message_reactions (message_id, user_id, reaction) VALUES ($1, $2, $3) "
        "ON CONFLICT (message_id, user_id, reaction) DO NOTHING");
    db_->prepareStatement("remove_message_reaction",
        "DELETE FROM channel_message_reactions WHERE message_id = $1 AND user_id = $2 AND reaction = $3");
    db_->prepareStatement("get_message_reactions",
        "SELECT reaction FROM channel_message_reactions WHERE message_id = $1");
    db_->prepareStatement("add_message_view",
        "INSERT INTO channel_message_views (message_id, user_id) VALUES ($1, $2) "
        "ON CONFLICT (message_id, user_id) DO NOTHING");
    db_->prepareStatement("get_message_views_count",
        "SELECT COUNT(*) FROM channel_message_views WHERE message_id = $1");
    db_->prepareStatement("update_channel_message_views",
        "UPDATE channel_messages SET views_count = (SELECT COUNT(*) FROM channel_message_views WHERE message_id = $1) WHERE id = $1");
    db_->prepareStatement("remove_channel_member",
        "DELETE FROM channel_members WHERE channel_id = $1 AND user_id = $2");
    db_->prepareStatement("update_channel_member_role",
        "UPDATE channel_members SET role = $1 WHERE channel_id = $2 AND user_id = $3");
    db_->prepareStatement("ban_channel_member",
        "UPDATE channel_members SET is_banned = $1 WHERE channel_id = $2 AND user_id = $3");
    db_->prepareStatement("get_channel_members",
        "SELECT cm.id, cm.channel_id, cm.user_id, u.username, cm.role, cm.is_banned, cm.joined_at "
        "FROM channel_members cm JOIN users u ON cm.user_id = u.id WHERE cm.channel_id = $1");
    db_->prepareStatement("count_channel_subscribers",
        "SELECT COUNT(*) FROM channel_members WHERE channel_id = $1 AND role = 'subscriber'");
    db_->prepareStatement("count_channel_members",
        "SELECT COUNT(*) FROM channel_members WHERE channel_id = $1");
    db_->prepareStatement("get_channel_member",
        "SELECT cm.id, cm.channel_id, cm.user_id, u.username, cm.role, cm.is_banned, cm.joined_at "
        "FROM channel_members cm JOIN users u ON cm.user_id = u.id WHERE cm.channel_id = $1 AND cm.user_id = $2");
    // Legacy pin/unpin must be scoped to channel_id and match callers' param counts
    db_->prepareStatement("pin_channel_message",
        "UPDATE channel_messages SET is_pinned = TRUE WHERE channel_id = $1 AND id = $2");
    db_->prepareStatement("unpin_channel_message",
        "UPDATE channel_messages SET is_pinned = FALSE WHERE channel_id = $1 AND id = $2");
    db_->prepareStatement("get_channel_message_meta",
        "SELECT channel_id::text, sender_id::text FROM channel_messages WHERE id = $1");
    db_->prepareStatement("delete_channel_message",
        "DELETE FROM channel_messages WHERE id = $1 AND channel_id = $2");
    db_->prepareStatement("get_group_message_meta",
        "SELECT group_id::text, sender_id::text FROM group_messages WHERE id = $1");
    db_->prepareStatement("delete_group_message",
        "DELETE FROM group_messages WHERE id = $1 AND group_id = $2");
    db_->prepareStatement("set_channel_custom_link",
        "UPDATE channels SET custom_link = NULLIF($1, '') WHERE id = $2");
    db_->prepareStatement("update_channel_name",
        "UPDATE channels SET name = $1 WHERE id = $2");
    db_->prepareStatement("update_channel_description",
        "UPDATE channels SET description = $1 WHERE id = $2");
    db_->prepareStatement("set_channel_privacy",
        "UPDATE channels SET is_private = $1 WHERE id = $2");
    db_->prepareStatement("set_channel_show_author",
        "UPDATE channels SET show_author = $1 WHERE id = $2");
    db_->prepareStatement("delete_channel_legacy",
        "DELETE FROM channels WHERE id = $1");
    db_->prepareStatement("add_allowed_reaction",
        "INSERT INTO channel_allowed_reactions (channel_id, reaction) VALUES ($1, $2) "
        "ON CONFLICT (channel_id, reaction) DO NOTHING");
    db_->prepareStatement("remove_allowed_reaction",
        "DELETE FROM channel_allowed_reactions WHERE channel_id = $1 AND reaction = $2");
    db_->prepareStatement("get_allowed_reactions",
        "SELECT reaction FROM channel_allowed_reactions WHERE channel_id = $1");
    db_->prepareStatement("create_channel_join_request",
        "INSERT INTO channel_join_requests (channel_id, user_id) VALUES ($1, $2) "
        "ON CONFLICT (channel_id, user_id) DO NOTHING");
    db_->prepareStatement("accept_channel_join_request",
        "UPDATE channel_join_requests SET status = 'accepted' WHERE channel_id = $1 AND user_id = $2");
    db_->prepareStatement("reject_channel_join_request",
        "UPDATE channel_join_requests SET status = 'rejected' WHERE channel_id = $1 AND user_id = $2");
    db_->prepareStatement("get_channel_join_requests",
        "SELECT cjr.id, cjr.channel_id, cjr.user_id, u.username, cjr.created_at "
        "FROM channel_join_requests cjr JOIN users u ON cjr.user_id = u.id "
        "WHERE cjr.channel_id = $1 AND cjr.status = 'pending'");
    db_->prepareStatement("create_channel_chat",
        "INSERT INTO channel_chats (channel_id, group_id) VALUES ($1, $2) "
        "ON CONFLICT (channel_id) DO UPDATE SET group_id = EXCLUDED.group_id");
    
    // Marketplace statements
    db_->prepareStatement("create_market_product",
        "INSERT INTO market_products (owner_chat_id, owner_type, title, description, price_amount, price_currency, product_type, image_path, stock_quantity, created_by) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, NULLIF($8, ''), NULLIF($9::integer, NULL), $10) RETURNING id");
    db_->prepareStatement("update_market_product",
        "UPDATE market_products SET title = $1, description = $2, price_amount = $3, price_currency = $4, image_path = NULLIF($5, ''), stock_quantity = NULLIF($6::integer, NULL), is_active = $7 WHERE id = $8");
    db_->prepareStatement("delete_market_product",
        "DELETE FROM market_products WHERE id = $1");
    db_->prepareStatement("get_market_products",
        "SELECT id, owner_chat_id, owner_type, title, description, price_amount, price_currency, product_type, image_path, is_active, stock_quantity, created_at, created_by "
        "FROM market_products WHERE owner_chat_id = $1 AND owner_type = $2 AND is_active = TRUE ORDER BY created_at DESC");
    db_->prepareStatement("get_market_product_by_id",
        "SELECT id, owner_chat_id, owner_type, title, description, price_amount, price_currency, product_type, image_path, is_active, stock_quantity, created_at, created_by "
        "FROM market_products WHERE id = $1");
    db_->prepareStatement("add_product_key",
        "INSERT INTO product_keys (product_id, key_value) VALUES ($1, $2)");
    db_->prepareStatement("get_unused_product_key",
        "SELECT id, key_value FROM product_keys WHERE product_id = $1 AND is_used = FALSE LIMIT 1 FOR UPDATE SKIP LOCKED");
    db_->prepareStatement("mark_product_key_used",
        "UPDATE product_keys SET is_used = TRUE, used_by = $2, used_at = CURRENT_TIMESTAMP WHERE id = $1");
    db_->prepareStatement("create_product_purchase",
        "INSERT INTO product_purchases (product_id, buyer_id, seller_id, price_amount, price_currency, platform_fee, invoice_id, status) "
        "VALUES ($1, $2, $3, $4, $5, $4 * 0.05, $6, 'paid') RETURNING id");
    db_->prepareStatement("update_purchase_key",
        "UPDATE product_purchases SET key_id = $2, status = 'delivered', delivered_at = CURRENT_TIMESTAMP WHERE id = $1");
    db_->prepareStatement("get_user_purchases",
        "SELECT id, product_id, buyer_id, seller_id, price_amount, price_currency, platform_fee, status, invoice_id, key_id, created_at "
        "FROM product_purchases WHERE buyer_id = $1 ORDER BY created_at DESC");
    
    // Integration statements
    db_->prepareStatement("create_integration_binding",
        "INSERT INTO integration_bindings (chat_id, chat_type, service_name, external_token, config_json, created_by) "
        "VALUES ($1, $2, $3, $4, $5::jsonb, $6) RETURNING id");
    db_->prepareStatement("update_integration_binding",
        "UPDATE integration_bindings SET config_json = $1::jsonb, is_active = $2 WHERE id = $3");
    db_->prepareStatement("delete_integration_binding",
        "DELETE FROM integration_bindings WHERE id = $1");
    db_->prepareStatement("get_integration_bindings",
        "SELECT id, chat_id, chat_type, service_name, config_json, is_active, created_at, created_by "
        "FROM integration_bindings WHERE chat_id = $1 AND chat_type = $2");
    db_->prepareStatement("get_integration_binding_by_id",
        "SELECT id, chat_id, chat_type, service_name, config_json, is_active, created_at, created_by "
        "FROM integration_bindings WHERE id = $1");
    
    // Bot Builder statements
    db_->prepareStatement("create_bot_builder_bot",
        "INSERT INTO bot_builder_bots (user_id, bot_user_id, bot_token, bot_username, bot_name, flow_json) "
        "VALUES ($1, $2::uuid, $3, $4, $5, $6::jsonb) RETURNING id");
    db_->prepareStatement("update_bot_builder_bot",
        "UPDATE bot_builder_bots SET flow_json = $1::jsonb, is_active = $2, updated_at = CURRENT_TIMESTAMP WHERE id = $3");
    db_->prepareStatement("delete_bot_builder_bot",
        "DELETE FROM bot_builder_bots WHERE id = $1");
    
    // Bot developers
    db_->prepareStatement("add_bot_developer",
        "INSERT INTO bot_developers (bot_id, developer_user_id, added_by_user_id) "
        "VALUES ($1::uuid, $2::uuid, $3::uuid) "
        "ON CONFLICT (bot_id, developer_user_id) DO NOTHING");
    db_->prepareStatement("remove_bot_developer",
        "DELETE FROM bot_developers WHERE bot_id = $1::uuid AND developer_user_id = $2::uuid");
    db_->prepareStatement("get_bot_developers",
        "SELECT d.developer_user_id, u.username, d.added_by_user_id, d.added_at "
        "FROM bot_developers d "
        "JOIN users u ON u.id = d.developer_user_id "
        "WHERE d.bot_id = $1::uuid "
        "ORDER BY d.added_at ASC");
    db_->prepareStatement("is_bot_developer",
        "SELECT 1 FROM bot_developers WHERE bot_id = $1::uuid AND developer_user_id = $2::uuid LIMIT 1");
    
    // Bot files
    db_->prepareStatement("upsert_bot_file",
        "INSERT INTO bot_files (bot_id, file_path, file_content) "
        "VALUES ($1::uuid, $2, $3) "
        "ON CONFLICT (bot_id, file_path) "
        "DO UPDATE SET file_content = $3, updated_at = CURRENT_TIMESTAMP");
    db_->prepareStatement("get_bot_file",
        "SELECT file_content FROM bot_files WHERE bot_id = $1::uuid AND file_path = $2");
    db_->prepareStatement("delete_bot_file",
        "DELETE FROM bot_files WHERE bot_id = $1::uuid AND file_path = $2");
    db_->prepareStatement("list_bot_files",
        "SELECT id, file_path, file_content, created_at, updated_at "
        "FROM bot_files WHERE bot_id = $1::uuid ORDER BY file_path ASC");
    
    db_->prepareStatement("get_user_bots",
        "SELECT id, user_id, COALESCE(bot_user_id::text, ''), bot_token, bot_username, bot_name, flow_json, is_active, created_at, deployed_at, "
        "COALESCE(bot_description, ''), COALESCE(bot_avatar_url, '') "
        "FROM bot_builder_bots WHERE user_id = $1 ORDER BY created_at DESC");
    db_->prepareStatement("get_bot_builder_bot_by_token",
        "SELECT id, user_id, COALESCE(bot_user_id::text, ''), bot_token, bot_username, bot_name, flow_json, is_active, created_at, deployed_at, "
        "COALESCE(bot_description, ''), COALESCE(bot_avatar_url, '') "
        "FROM bot_builder_bots WHERE bot_token = $1");
    db_->prepareStatement("get_bot_builder_bot_by_username",
        "SELECT id, user_id, COALESCE(bot_user_id::text, ''), bot_token, bot_username, bot_name, flow_json, is_active, created_at, deployed_at, "
        "COALESCE(bot_description, ''), COALESCE(bot_avatar_url, '') "
        "FROM bot_builder_bots WHERE bot_username = $1");
    db_->prepareStatement("get_bot_builder_bot_by_id",
        "SELECT id, user_id, COALESCE(bot_user_id::text, ''), bot_token, bot_username, bot_name, flow_json, is_active, created_at, deployed_at, "
        "COALESCE(bot_description, ''), COALESCE(bot_avatar_url, '') "
        "FROM bot_builder_bots WHERE id = $1");
    db_->prepareStatement("get_bot_builder_bot_by_user_id",
        "SELECT id, user_id, COALESCE(bot_user_id::text, ''), bot_token, bot_username, bot_name, flow_json, is_active, created_at, deployed_at, "
        "COALESCE(bot_description, ''), COALESCE(bot_avatar_url, '') "
        "FROM bot_builder_bots WHERE bot_user_id = $1::uuid");
    db_->prepareStatement("update_bot_builder_profile",
        "UPDATE bot_builder_bots SET bot_name = $1, bot_description = $2, updated_at = CURRENT_TIMESTAMP WHERE id = $3");
    db_->prepareStatement("update_bot_builder_avatar",
        "UPDATE bot_builder_bots SET bot_avatar_url = $1, updated_at = CURRENT_TIMESTAMP WHERE id = $2");
    db_->prepareStatement("get_bot_builder_script_by_bot_user_id",
        "SELECT COALESCE(script_lang, 'lite'), COALESCE(script_enabled, FALSE), COALESCE(script_code, '') "
        "FROM bot_builder_bots WHERE bot_user_id = $1::uuid LIMIT 1");
    db_->prepareStatement("get_bot_builder_script_by_id",
        "SELECT COALESCE(script_lang, 'lite'), COALESCE(script_enabled, FALSE), COALESCE(script_code, '') "
        "FROM bot_builder_bots WHERE id = $1::uuid LIMIT 1");
    db_->prepareStatement("update_bot_builder_script",
        "UPDATE bot_builder_bots SET script_lang = $1, script_enabled = $2::boolean, script_code = $3, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = $4::uuid");
    db_->prepareStatement("log_bot_execution",
        "INSERT INTO bot_execution_logs (bot_id, update_id, user_id, chat_id, node_id, node_type, execution_result, execution_time_ms, error_message) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb, $8, $9)");

    // Bot runtime: notes
    db_->prepareStatement("upsert_bot_note",
        "INSERT INTO bot_notes (bot_user_id, scope_type, scope_id, note_key, note_value, created_by) "
        "VALUES ($1::uuid, $2, $3::uuid, $4, $5, NULLIF($6,'')::uuid) "
        "ON CONFLICT (bot_user_id, scope_type, scope_id, note_key) "
        "DO UPDATE SET note_value = EXCLUDED.note_value, updated_at = now(), created_by = EXCLUDED.created_by");
    db_->prepareStatement("get_bot_note",
        "SELECT note_value FROM bot_notes "
        "WHERE bot_user_id = $1::uuid AND scope_type = $2 AND scope_id = $3::uuid AND note_key = $4 "
        "LIMIT 1");
    db_->prepareStatement("list_bot_notes",
        "SELECT note_key FROM bot_notes "
        "WHERE bot_user_id = $1::uuid AND scope_type = $2 AND scope_id = $3::uuid "
        "ORDER BY note_key ASC LIMIT 200");
    db_->prepareStatement("delete_bot_note",
        "DELETE FROM bot_notes "
        "WHERE bot_user_id = $1::uuid AND scope_type = $2 AND scope_id = $3::uuid AND note_key = $4");

    // Bot runtime: reminders
    db_->prepareStatement("create_bot_reminder",
        "INSERT INTO bot_reminders (bot_user_id, scope_type, scope_id, target_user_id, reminder_text, due_at) "
        "VALUES ($1::uuid, $2, $3::uuid, NULLIF($4,'')::uuid, $5, now() + ($6::text || ' seconds')::interval) "
        "RETURNING id::text");
    db_->prepareStatement("claim_due_bot_reminders",
        "WITH due AS ("
        "  SELECT id FROM bot_reminders "
        "  WHERE sent_at IS NULL AND due_at <= now() "
        "  ORDER BY due_at ASC "
        "  LIMIT $1"
        ") "
        "UPDATE bot_reminders br "
        "SET sent_at = now() "
        "WHERE br.id IN (SELECT id FROM due) "
        "RETURNING br.id::text, br.bot_user_id::text, br.scope_type, br.scope_id::text, COALESCE(br.target_user_id::text,''), br.reminder_text");

    // Bot runtime: group bots (members)
    db_->prepareStatement("get_group_bot_user_ids",
        "SELECT gm.user_id::text "
        "FROM group_members gm "
        "JOIN bot_builder_bots b ON b.bot_user_id = gm.user_id "
        "WHERE gm.group_id = $1::uuid AND b.is_active = TRUE");
    
    // Event Router statements
    db_->prepareStatement("create_trigger_rule",
        "INSERT INTO trigger_rules (chat_id, chat_type, rule_name, trigger_conditions, actions, rate_limit_per_second, created_by) "
        "VALUES ($1, $2, $3, $4::jsonb, $5::jsonb, $6, $7) RETURNING id");
    db_->prepareStatement("update_trigger_rule",
        "UPDATE trigger_rules SET trigger_conditions = $1::jsonb, actions = $2::jsonb, is_active = $3, rate_limit_per_second = $4 WHERE id = $5");
    db_->prepareStatement("delete_trigger_rule",
        "DELETE FROM trigger_rules WHERE id = $1");
    db_->prepareStatement("get_trigger_rules",
        "SELECT id, chat_id, chat_type, rule_name, trigger_conditions, actions, is_active, rate_limit_per_second, created_at, created_by "
        "FROM trigger_rules WHERE chat_id = $1 AND chat_type = $2 AND is_active = TRUE");
    db_->prepareStatement("create_event_webhook",
        "INSERT INTO event_webhooks (trigger_rule_id, webhook_url, secret_token, http_method, headers_json) "
        "VALUES ($1, $2, $3, $4, $5::jsonb) RETURNING id");
    db_->prepareStatement("log_trigger_execution",
        "INSERT INTO trigger_executions (trigger_rule_id, event_type, event_data, execution_result, status, error_message) "
        "VALUES ($1, $2, $3::jsonb, $4::jsonb, $5, $6)");

    // Advanced chats/supergroups (v2 unified)
    db_->prepareStatement("create_chat_v2",
        "INSERT INTO chats (type, title, username, about, is_public, slow_mode_sec, sign_messages, created_by) "
        "VALUES ($1::chat_type, $2, NULLIF($3, ''), NULLIF($4, ''), $5::boolean, NULLIF($6, '0')::integer, $7::boolean, $8::uuid) "
        "RETURNING id");
    db_->prepareStatement("update_chat_title_v2",
        "UPDATE chats SET title = $1 WHERE id = $2::uuid");
    db_->prepareStatement("update_chat_about_v2",
        "UPDATE chats SET about = $1 WHERE id = $2::uuid");
    db_->prepareStatement("update_chat_username_v2",
        "UPDATE chats SET username = NULLIF($1, '') WHERE id = $2::uuid");
    db_->prepareStatement("update_chat_is_public_v2",
        "UPDATE chats SET is_public = $1::boolean WHERE id = $2::uuid");
    db_->prepareStatement("update_chat_sign_messages_v2",
        "UPDATE chats SET sign_messages = $1::boolean WHERE id = $2::uuid");
    db_->prepareStatement("insert_chat_sequence_v2",
        "INSERT INTO chat_sequences (chat_id, next_local) VALUES ($1::uuid, 1) ON CONFLICT (chat_id) DO NOTHING");
    db_->prepareStatement("insert_chat_participant_owner_v2",
        "INSERT INTO chat_participants (chat_id, user_id, status) VALUES ($1::uuid, $2::uuid, 'owner') "
        "ON CONFLICT (chat_id, user_id) DO UPDATE SET status = 'owner', admin_role_id = NULL");
    db_->prepareStatement("upsert_chat_member_v2",
        "INSERT INTO chat_participants (chat_id, user_id, status) VALUES ($1::uuid, $2::uuid, 'member') "
        "ON CONFLICT (chat_id, user_id) DO UPDATE SET status = 'member', admin_role_id = NULL");
    db_->prepareStatement("leave_chat_v2",
        "UPDATE chat_participants SET status = 'left', admin_role_id = NULL WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("get_chat_type_v2",
        "SELECT type FROM chats WHERE id = $1::uuid");
    db_->prepareStatement("insert_chat_link_v2",
        "INSERT INTO chat_links (channel_id, discussion_id) VALUES ($1::uuid, $2::uuid) "
        "ON CONFLICT (channel_id) DO UPDATE SET discussion_id = EXCLUDED.discussion_id");
    db_->prepareStatement("upsert_admin_role_v2",
        "INSERT INTO admin_roles (chat_id, title, perms) VALUES ($1::uuid, $2, $3::bigint) "
        "ON CONFLICT (chat_id, title) DO UPDATE SET perms = EXCLUDED.perms "
        "RETURNING id, perms");
    db_->prepareStatement("upsert_participant_admin_v2",
        "INSERT INTO chat_participants (chat_id, user_id, status, admin_role_id) VALUES ($1::uuid, $2::uuid, 'admin', $3::uuid) "
        "ON CONFLICT (chat_id, user_id) DO UPDATE SET status = 'admin', admin_role_id = EXCLUDED.admin_role_id");
    db_->prepareStatement("set_owner_adminrole_v2",
        "UPDATE chat_participants SET admin_role_id = $2::uuid WHERE chat_id = $1::uuid AND user_id = $3::uuid");
    db_->prepareStatement("get_participant_role_v2",
        "SELECT status, admin_role_id FROM chat_participants WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("get_admin_role_perms_v2",
        "SELECT perms FROM admin_roles WHERE id = $1::uuid");
    db_->prepareStatement("get_participant_restriction_v2",
        "SELECT can_send_messages, can_send_media, can_add_links, can_invite_users, until "
        "FROM participant_restrictions "
        "WHERE chat_id = $1::uuid AND user_id = $2::uuid AND (until IS NULL OR until > now())");
    db_->prepareStatement("get_chat_v2",
        "SELECT id, type, title, COALESCE(username, ''), COALESCE(about, ''), COALESCE(avatar_url, ''), is_public, "
        "COALESCE(sign_messages, true), COALESCE(slow_mode_sec, 0), created_by, created_at, COALESCE(is_verified, false) "
        "FROM chats WHERE id = $1::uuid");
    db_->prepareStatement("get_chat_by_username_v2",
        "SELECT id, type, title, COALESCE(username, ''), COALESCE(about, ''), COALESCE(avatar_url, ''), is_public, "
        "COALESCE(sign_messages, true), COALESCE(slow_mode_sec, 0), created_by, created_at, COALESCE(is_verified, false) "
        "FROM chats WHERE username = $1");
    db_->prepareStatement("get_user_channels_v2",
        "SELECT c.id, c.title, COALESCE(c.about, ''), COALESCE(c.username, ''), COALESCE(c.avatar_url, ''), c.is_public, "
        "COALESCE(c.sign_messages, true), c.created_by, c.created_at, COALESCE(c.is_verified, false) "
        "FROM chats c "
        "JOIN chat_participants cp ON cp.chat_id = c.id "
        "WHERE c.type = 'channel' AND cp.user_id = $1::uuid AND cp.status NOT IN ('kicked','left') "
        "ORDER BY c.created_at DESC");
    db_->prepareStatement("get_channel_members_v2",
        "SELECT cp.user_id, COALESCE(u.username, ''), cp.status::text, cp.joined_at::text, "
        "       COALESCE(ar.perms, 0), COALESCE(ar.title, '') "
        "FROM chat_participants cp "
        "LEFT JOIN users u ON u.id = cp.user_id "
        "LEFT JOIN admin_roles ar ON ar.id = cp.admin_role_id "
        "WHERE cp.chat_id = $1::uuid AND cp.status != 'left'");
    db_->prepareStatement("get_chat_link_v2",
        "SELECT discussion_id FROM chat_links WHERE channel_id = $1::uuid");
    db_->prepareStatement("get_chat_participant_v2",
        "SELECT status::text, admin_role_id::text, joined_at::text "
        "FROM chat_participants WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("update_chat_participant_status_v2",
        "UPDATE chat_participants SET status = $3::participant_status, admin_role_id = NULL "
        "WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("demote_chat_admin_v2",
        "UPDATE chat_participants SET status = 'member', admin_role_id = NULL "
        "WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("update_chat_avatar_v2",
        "UPDATE chats SET avatar_url = NULLIF($1, '') WHERE id = $2::uuid");
    db_->prepareStatement("delete_chat_v2",
        "DELETE FROM chats WHERE id = $1::uuid");
    db_->prepareStatement("update_channel_avatar",
        "UPDATE channels SET avatar_url = NULLIF($1, '') WHERE id = $2");
    db_->prepareStatement("inc_local_id_v2",
        "UPDATE chat_sequences SET next_local = next_local + 1 WHERE chat_id = $1::uuid RETURNING next_local");
    db_->prepareStatement("insert_chat_message_v2",
        "INSERT INTO chat_messages (chat_id, local_id, sender_id, replied_to, thread_root_id, content_type, content, is_silent) "
        "VALUES ($1::uuid, $2::bigint, $3::uuid, $4::bigint, $5::bigint, $6, $7::jsonb, $8::boolean) "
        "RETURNING id");
    // Reactions v2
    db_->prepareStatement("insert_reaction_counts_v2",
        "INSERT INTO message_reaction_counts (message_id, counts) VALUES ($1::bigint, '{}'::jsonb) ON CONFLICT DO NOTHING");
    db_->prepareStatement("insert_reaction_shard_v2",
        "INSERT INTO message_reactions_shard (shard, message_id, user_id, reaction) VALUES ($1::smallint, $2::bigint, $3::uuid, $4) "
        "ON CONFLICT DO NOTHING");
    db_->prepareStatement("delete_reaction_shard_v2",
        "DELETE FROM message_reactions_shard WHERE shard = $1::smallint AND message_id = $2::bigint AND user_id = $3::uuid AND reaction = $4");
    db_->prepareStatement("update_reaction_count_v2",
        "UPDATE message_reaction_counts SET counts = jsonb_set(COALESCE(counts, '{}'::jsonb), "
        "ARRAY[$2], to_jsonb(GREATEST(0, COALESCE((counts->>$2)::bigint, 0) + $3))) "
        "WHERE message_id = $1::bigint RETURNING counts");
    db_->prepareStatement("get_reaction_counts_v2",
        "SELECT counts FROM message_reaction_counts WHERE message_id = $1::bigint");
    db_->prepareStatement("get_reaction_counts_kv_v2",
        "SELECT key, (value)::bigint FROM message_reaction_counts, LATERAL jsonb_each(counts) WHERE message_id = $1::bigint");
    db_->prepareStatement("get_chat_messages_v2",
        "SELECT m.id, m.local_id, m.sender_id, COALESCE(u.username, ''), m.content, m.content_type, m.is_silent, m.created_at, "
        "       (cp.message_id IS NOT NULL) AS is_pinned "
        "FROM chat_messages m "
        "LEFT JOIN users u ON u.id = m.sender_id "
        "LEFT JOIN chat_pins cp ON cp.chat_id = m.chat_id AND cp.message_id = m.id "
        "WHERE m.chat_id = $1::uuid "
        "AND ($2::bigint = 0 OR local_id < $2::bigint) "
        "ORDER BY local_id DESC LIMIT $3");

    db_->prepareStatement("pin_chat_message_v2",
        "INSERT INTO chat_pins (chat_id, message_id, pinned_by) "
        "VALUES ($1::uuid, $2::bigint, $3::uuid) "
        "ON CONFLICT (chat_id) DO UPDATE "
        "SET message_id = EXCLUDED.message_id, pinned_by = EXCLUDED.pinned_by, pinned_at = now()");
    db_->prepareStatement("unpin_chat_message_v2",
        "DELETE FROM chat_pins WHERE chat_id = $1::uuid");
    db_->prepareStatement("get_chat_message_meta_v2",
        "SELECT chat_id::text, COALESCE(sender_id::text, '') "
        "FROM chat_messages WHERE id = $1::bigint");
    db_->prepareStatement("delete_chat_message_v2",
        "DELETE FROM chat_messages WHERE id = $1::bigint AND chat_id = $2::uuid");

    db_->prepareStatement("upsert_channel_read_state_v2",
        "INSERT INTO channel_read_states (channel_id, user_id, max_read_local) "
        "VALUES ($1::uuid, $2::uuid, $3::bigint) "
        "ON CONFLICT (channel_id, user_id) "
        "DO UPDATE SET max_read_local = GREATEST(channel_read_states.max_read_local, EXCLUDED.max_read_local), "
        "updated_at = now()");

    db_->prepareStatement("get_channel_unread_v2",
        "SELECT (COALESCE(cs.next_local,1)-1) - COALESCE(crs.max_read_local,0) AS unread "
        "FROM channel_sequences cs "
        "LEFT JOIN channel_read_states crs ON crs.channel_id = cs.channel_id AND crs.user_id = $2::uuid "
        "WHERE cs.channel_id = $1::uuid");

    // Join requests + reactions (v2 chats)
    db_->prepareStatement("create_chat_join_request",
        "INSERT INTO chat_join_requests (chat_id, user_id) VALUES ($1::uuid, $2::uuid) "
        "ON CONFLICT (chat_id, user_id) DO NOTHING");
    db_->prepareStatement("accept_chat_join_request",
        "UPDATE chat_join_requests SET status = 'accepted' WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("reject_chat_join_request",
        "UPDATE chat_join_requests SET status = 'rejected' WHERE chat_id = $1::uuid AND user_id = $2::uuid");
    db_->prepareStatement("get_chat_join_requests",
        "SELECT cjr.id, cjr.chat_id, cjr.user_id, COALESCE(u.username, ''), cjr.created_at "
        "FROM chat_join_requests cjr JOIN users u ON u.id = cjr.user_id "
        "WHERE cjr.chat_id = $1::uuid AND cjr.status = 'pending'");
    db_->prepareStatement("add_chat_allowed_reaction",
        "INSERT INTO chat_allowed_reactions (chat_id, reaction) VALUES ($1::uuid, $2) "
        "ON CONFLICT (chat_id, reaction) DO NOTHING");
    db_->prepareStatement("remove_chat_allowed_reaction",
        "DELETE FROM chat_allowed_reactions WHERE chat_id = $1::uuid AND reaction = $2");
    db_->prepareStatement("get_chat_allowed_reactions",
        "SELECT reaction FROM chat_allowed_reactions WHERE chat_id = $1::uuid");

    // Chat invites (v2)
    db_->prepareStatement("create_chat_invite_v2",
        "INSERT INTO chat_invite_links (chat_id, token, expire_at, usage_limit, created_by) "
        "VALUES ($1::uuid, uuid_generate_v4()::text, "
        "CASE WHEN $3::int > 0 THEN now() + ($3::text || ' seconds')::interval ELSE NULL END, "
        "NULLIF($4::int, 0), $2::uuid) RETURNING token");
    db_->prepareStatement("revoke_chat_invite_v2",
        "UPDATE chat_invite_links SET is_revoked = TRUE WHERE token = $1");
    db_->prepareStatement("get_chat_invite_v2",
        "SELECT chat_id, expire_at, usage_limit, usage_count, is_revoked FROM chat_invite_links WHERE token = $1");
    db_->prepareStatement("inc_chat_invite_usage_v2",
        "UPDATE chat_invite_links SET usage_count = usage_count + 1 WHERE token = $1");

    // Channel invites (legacy)
    db_->prepareStatement("create_channel_invite_legacy",
        "INSERT INTO channel_invite_links (channel_id, token, expire_at, usage_limit, created_by) "
        "VALUES ($1::uuid, uuid_generate_v4()::text, "
        "CASE WHEN $3::int > 0 THEN now() + ($3::text || ' seconds')::interval ELSE NULL END, "
        "NULLIF($4::int, 0), $2::uuid) RETURNING token");
    db_->prepareStatement("revoke_channel_invite_legacy",
        "UPDATE channel_invite_links SET is_revoked = TRUE WHERE token = $1");
    db_->prepareStatement("get_channel_invite_legacy",
        "SELECT channel_id, expire_at, usage_limit, usage_count, is_revoked FROM channel_invite_links WHERE token = $1");
    db_->prepareStatement("inc_channel_invite_usage_legacy",
        "UPDATE channel_invite_links SET usage_count = usage_count + 1 WHERE token = $1");
    db_->prepareStatement("get_channel_subscribers_v2",
        "SELECT user_id FROM chat_participants WHERE chat_id = $1::uuid AND status IN ('owner','admin','member')");
    db_->prepareStatement("count_chat_subscribers_v2",
        "SELECT COUNT(*) FROM chat_participants WHERE chat_id = $1::uuid AND status IN ('owner','admin','member')");
    db_->prepareStatement("count_chat_members_v2",
        "SELECT COUNT(*) FROM chat_participants WHERE chat_id = $1::uuid AND status != 'left'");

    // Polls v2 (supports chat, channel, group)
    db_->prepareStatement("insert_poll_v2",
        "INSERT INTO polls (message_id, message_type, question, is_anonymous, allows_multiple, closes_at) "
        "VALUES ($1::bigint, $2, $3, $4::boolean, $5::boolean, NULLIF($6,'')::timestamptz) RETURNING id");
    db_->prepareStatement("insert_poll_option_v2",
        "INSERT INTO poll_options (poll_id, option_text) VALUES ($1::bigint, $2) RETURNING id");
    db_->prepareStatement("vote_poll_v2",
        "INSERT INTO poll_votes (poll_id, option_id, user_id) VALUES ($1::bigint, $2::bigint, $3::uuid) "
        "ON CONFLICT DO NOTHING");
    db_->prepareStatement("inc_poll_option_v2",
        "UPDATE poll_options SET vote_count = vote_count + 1 WHERE id = $1::bigint RETURNING vote_count");
    db_->prepareStatement("get_poll_v2",
        "SELECT p.id, p.message_id, p.message_type, p.question, p.is_anonymous, p.allows_multiple, p.closes_at, "
        "po.id, po.option_text, po.vote_count "
        "FROM polls p JOIN poll_options po ON p.id = po.poll_id WHERE p.id = $1::bigint");
    db_->prepareStatement("get_poll_by_message",
        "SELECT p.id, p.message_id, p.message_type, p.question, p.is_anonymous, p.allows_multiple, p.closes_at, "
        "po.id, po.option_text, po.vote_count "
        "FROM polls p JOIN poll_options po ON p.id = po.poll_id "
        "WHERE p.message_type = $1 AND p.message_id = $2::bigint");

    // Moderation reports
    db_->prepareStatement("create_message_report",
        "INSERT INTO message_reports (reporter_id, reported_user_id, message_id, message_type, message_content, reason, comment, context_json) "
        "VALUES ($1::uuid, $2::uuid, $3, $4, $5, $6, NULLIF($7, ''), COALESCE($8::jsonb, '[]'::jsonb)) RETURNING id");
    db_->prepareStatement("get_message_reports_paginated",
        "SELECT r.id, r.reporter_id, COALESCE(rep.username, '') as reporter_username, "
        "r.reported_user_id, COALESCE(u.username, '') as reported_username, "
        "r.message_id, r.message_type, r.message_content, r.reason, COALESCE(r.comment,''), "
        "r.context_json, r.status, COALESCE(r.resolution_note,''), r.resolved_by, "
        "r.resolved_at, r.created_at "
        "FROM message_reports r "
        "LEFT JOIN users u ON u.id = r.reported_user_id "
        "LEFT JOIN users rep ON rep.id = r.reporter_id "
        "WHERE ($3 = '' OR r.status = $3) "
        "ORDER BY r.created_at DESC "
        "LIMIT $1 OFFSET $2");
    db_->prepareStatement("count_message_reports",
        "SELECT COUNT(*) FROM message_reports WHERE ($1 = '' OR status = $1)");
    db_->prepareStatement("count_reports_for_user_window",
        "SELECT COUNT(*) FROM message_reports "
        "WHERE reported_user_id = $1::uuid "
        "AND ($2 = '' OR reason = $2) "
        "AND status <> 'dismissed' "
        "AND created_at >= (now() - ($3::text || ' minutes')::interval)");
    db_->prepareStatement("get_message_report_by_id",
        "SELECT r.id, r.reporter_id, COALESCE(rep.username, '') as reporter_username, "
        "r.reported_user_id, COALESCE(u.username, '') as reported_username, "
        "r.message_id, r.message_type, r.message_content, r.reason, COALESCE(r.comment,''), "
        "r.context_json, r.status, COALESCE(r.resolution_note,''), r.resolved_by, "
        "r.resolved_at, r.created_at "
        "FROM message_reports r "
        "LEFT JOIN users u ON u.id = r.reported_user_id "
        "LEFT JOIN users rep ON rep.id = r.reporter_id "
        "WHERE r.id = $1::uuid");
    db_->prepareStatement("update_message_report_status",
        "UPDATE message_reports SET status = $2, resolved_by = NULLIF($3, '')::uuid, "
        "resolution_note = NULLIF($4, ''), "
        "resolved_at = CASE WHEN $2 IN ('resolved','dismissed') THEN NOW() ELSE resolved_at END "
        "WHERE id = $1::uuid");
    db_->prepareStatement("get_message_context",
        "SELECT id, sender_id, receiver_id, content, created_at, is_read, is_delivered, message_type, file_path, file_name, file_size, reply_to_message_id "
        "FROM messages "
        "WHERE ((sender_id = $1::uuid AND receiver_id = $2::uuid) OR (sender_id = $2::uuid AND receiver_id = $1::uuid)) "
        "AND created_at BETWEEN (SELECT created_at FROM messages WHERE id = $3::uuid) - INTERVAL '15 minutes' "
        "AND (SELECT created_at FROM messages WHERE id = $3::uuid) + INTERVAL '15 minutes' "
        "ORDER BY created_at ASC LIMIT $4");
}

bool DatabaseManager::createUser(const std::string& username, const std::string& password_hash) {
    const char* param_values[2] = {username.c_str(), password_hash.c_str()};
    PGresult* res = db_->executePrepared("create_user", 2, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::createBotUser(const std::string& username, const std::string& password_hash, std::string& out_user_id) {
    const char* param_values[2] = {username.c_str(), password_hash.c_str()};
    PGresult* res = db_->executePrepared("create_bot_user", 2, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        Logger::getInstance().error("Failed to create bot user '" + username + "': " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    out_user_id = PQgetvalue(res, 0, 0);
    PQclear(res);
    return !out_user_id.empty();
}

User DatabaseManager::getUserByUsername(const std::string& username) {
    User user;
    const char* param_values[1] = {username.c_str()};
    
    PGresult* res = db_->executePrepared("get_user_by_username", 1, param_values);
    if (!res) {
        // Prepared statements may be lost after DB reconnect; re-prepare once and retry
        Logger::getInstance().warning("Prepared statement get_user_by_username missing, re-preparing statements");
        prepareStatements();
        res = db_->executePrepared("get_user_by_username", 1, param_values);
        if (!res) {
            return user;
        }
    }
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return user;
    }
    
    user.id = PQgetvalue(res, 0, 0);
    user.username = PQgetvalue(res, 0, 1);
    user.password_hash = PQgetvalue(res, 0, 2);
    user.created_at = PQgetvalue(res, 0, 3);
    user.is_active = (strcmp(PQgetvalue(res, 0, 4), "t") == 0);
    user.is_admin = (strcmp(PQgetvalue(res, 0, 5), "t") == 0);
    user.role = PQgetvalue(res, 0, 6);
    user.totp_secret = PQgetvalue(res, 0, 7);
    user.admin_ip_whitelist = PQgetvalue(res, 0, 8);
    user.is_bot = (strcmp(PQgetvalue(res, 0, 9), "t") == 0);
    user.is_premium = (strcmp(PQgetvalue(res, 0, 10), "t") == 0);
    user.premium_plan = PQgetvalue(res, 0, 11);
    user.premium_expires_at = PQgetvalue(res, 0, 12);
    
    PQclear(res);
    return user;
}

User DatabaseManager::getUserById(const std::string& user_id) {
    User user;
    const char* param_values[1] = {user_id.c_str()};
    
    PGresult* res = db_->executePrepared("get_user_by_id", 1, param_values);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return user;
    }
    
    user.id = PQgetvalue(res, 0, 0);
    user.username = PQgetvalue(res, 0, 1);
    user.password_hash = PQgetvalue(res, 0, 2);
    user.created_at = PQgetvalue(res, 0, 3);
    user.is_active = (strcmp(PQgetvalue(res, 0, 4), "t") == 0);
    user.is_admin = (strcmp(PQgetvalue(res, 0, 5), "t") == 0);
    user.role = PQgetvalue(res, 0, 6);
    user.totp_secret = PQgetvalue(res, 0, 7);
    user.admin_ip_whitelist = PQgetvalue(res, 0, 8);
    user.is_bot = (strcmp(PQgetvalue(res, 0, 9), "t") == 0);
    user.is_premium = (strcmp(PQgetvalue(res, 0, 10), "t") == 0);
    user.premium_plan = PQgetvalue(res, 0, 11);
    user.premium_expires_at = PQgetvalue(res, 0, 12);
    
    PQclear(res);
    return user;
}

bool DatabaseManager::usernameExists(const std::string& username) {
    const char* param_values[1] = {username.c_str()};
    
    PGresult* res = db_->executePrepared("check_username_exists", 1, param_values);
    if (!res) {
        return false;
    }
    
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count > 0;
}

bool DatabaseManager::updateLastLogin(const std::string& user_id) {
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("update_last_login", 1, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::updateLastActivity(const std::string& user_id) {
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("update_last_activity", 1, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::updateUserAvatar(const std::string& user_id, const std::string& avatar_url) {
    const char* param_values[2] = {avatar_url.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("update_user_avatar", 2, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::setUserPremium(const std::string& user_id, bool is_premium, const std::string& plan) {
    const char* params[3] = {
        user_id.c_str(),
        is_premium ? "true" : "false",
        plan.c_str()
    };
    PGresult* res = db_->executePrepared("set_user_premium", 3, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::createPremiumPayment(const std::string& user_id,
                                           const std::string& plan,
                                           const std::string& amount,
                                           PremiumPayment& out_payment) {
    const char* params[3] = {user_id.c_str(), plan.c_str(), amount.c_str()};
    PGresult* res = db_->executePrepared("create_premium_payment", 3, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_payment.id = PQgetvalue(res, 0, 0);
    out_payment.label = PQgetvalue(res, 0, 1);
    out_payment.plan = PQgetvalue(res, 0, 2);
    out_payment.amount = PQgetvalue(res, 0, 3);
    out_payment.status = PQgetvalue(res, 0, 4);
    out_payment.user_id = user_id;
    out_payment.gift_receiver_id.clear();
    PQclear(res);
    return true;
}

bool DatabaseManager::createPremiumGiftPayment(const std::string& user_id,
                                               const std::string& receiver_id,
                                               const std::string& plan,
                                               const std::string& amount,
                                               PremiumPayment& out_payment) {
    const char* params[4] = {user_id.c_str(), plan.c_str(), amount.c_str(), receiver_id.c_str()};
    PGresult* res = db_->executePrepared("create_premium_gift_payment", 4, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_payment.id = PQgetvalue(res, 0, 0);
    out_payment.label = PQgetvalue(res, 0, 1);
    out_payment.plan = PQgetvalue(res, 0, 2);
    out_payment.amount = PQgetvalue(res, 0, 3);
    out_payment.status = PQgetvalue(res, 0, 4);
    out_payment.gift_receiver_id = PQgetvalue(res, 0, 5);
    out_payment.user_id = user_id;
    PQclear(res);
    return true;
}

bool DatabaseManager::getPremiumPaymentByLabel(const std::string& label, PremiumPayment& out_payment) {
    const char* params[1] = {label.c_str()};
    PGresult* res = db_->executePrepared("get_premium_payment_by_label", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_payment.id = PQgetvalue(res, 0, 0);
    out_payment.user_id = PQgetvalue(res, 0, 1);
    out_payment.plan = PQgetvalue(res, 0, 2);
    out_payment.amount = PQgetvalue(res, 0, 3);
    out_payment.status = PQgetvalue(res, 0, 4);
    out_payment.operation_id = PQgetvalue(res, 0, 5);
    out_payment.payment_type = PQgetvalue(res, 0, 6);
    out_payment.gift_receiver_id = PQgetvalue(res, 0, 7);
    out_payment.created_at = PQgetvalue(res, 0, 8);
    out_payment.paid_at = PQgetvalue(res, 0, 9);
    out_payment.label = label;
    PQclear(res);
    return true;
}

bool DatabaseManager::markPremiumPaymentPaid(const std::string& label,
                                             const std::string& operation_id,
                                             const std::string& payment_type) {
    const char* params[3] = {label.c_str(), operation_id.c_str(), payment_type.c_str()};
    PGresult* res = db_->executePrepared("mark_premium_payment_paid", 3, params);
    if (!res) {
        return false;
    }
    bool updated = (PQntuples(res) > 0);
    PQclear(res);
    return updated;
}

bool DatabaseManager::resetPremiumPaymentStatus(const std::string& label) {
    const char* params[1] = {label.c_str()};
    PGresult* res = db_->executePrepared("reset_premium_payment_status", 1, params);
    if (!res) {
        return false;
    }
    bool updated = (PQntuples(res) > 0);
    PQclear(res);
    return updated;
}

bool DatabaseManager::hasTrialPayment(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("has_trial_payment", 1, params);
    if (!res) {
        return false;
    }
    bool exists = (PQntuples(res) > 0);
    PQclear(res);
    return exists;
}

std::vector<AdminPremiumPayment> DatabaseManager::listPremiumPayments(const std::string& status, int limit) {
    std::vector<AdminPremiumPayment> payments;
    if (limit < 1) limit = 20;
    if (limit > 100) limit = 100;
    const std::string status_value = status;
    const std::string limit_value = std::to_string(limit);
    const char* params[2] = {status_value.c_str(), limit_value.c_str()};
    PGresult* res = db_->executePrepared("admin_list_premium_payments", 2, params);
    if (!res) {
        return payments;
    }
    int rows = PQntuples(res);
    payments.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        AdminPremiumPayment payment;
        payment.id = PQgetvalue(res, i, 0);
        payment.label = PQgetvalue(res, i, 1);
        payment.plan = PQgetvalue(res, i, 2);
        payment.amount = PQgetvalue(res, i, 3);
        payment.status = PQgetvalue(res, i, 4);
        payment.operation_id = PQgetvalue(res, i, 5);
        payment.payment_type = PQgetvalue(res, i, 6);
        payment.created_at = PQgetvalue(res, i, 7);
        payment.paid_at = PQgetvalue(res, i, 8);
        payment.user_id = PQgetvalue(res, i, 9);
        payment.username = PQgetvalue(res, i, 10);
        payments.push_back(std::move(payment));
    }
    PQclear(res);
    return payments;
}

std::string DatabaseManager::getUserLastActivity(const std::string& user_id) {
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_last_activity", 1, param_values);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return "";
    }
    
    std::string last_activity = "";
    if (!PQgetisnull(res, 0, 0)) {
        last_activity = PQgetvalue(res, 0, 0);
    }
    
    PQclear(res);
    return last_activity;
}

bool DatabaseManager::isUserOnline(const std::string& user_id, int threshold_seconds) {
    const std::string threshold_str = std::to_string(threshold_seconds);
    const char* param_values[2] = {user_id.c_str(), threshold_str.c_str()};
    PGresult* res = db_->executePrepared("check_user_online", 2, param_values);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    
    bool is_online = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
    PQclear(res);
    return is_online;
}

bool DatabaseManager::upsertUserProfile(const UserProfile& profile) {
    const std::string bd = std::to_string(profile.birth_day);
    const std::string bm = std::to_string(profile.birth_month);
    const std::string by = std::to_string(profile.birth_year);
    const char* params[7] = {
        profile.user_id.c_str(),
        profile.first_name.c_str(),
        profile.last_name.c_str(),
        profile.bio.c_str(),
        bd.c_str(),
        bm.c_str(),
        by.c_str()
    };
    PGresult* res = db_->executePrepared("upsert_user_profile", 7, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::getUserProfile(const std::string& user_id, UserProfile& out_profile) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_profile", 1, params);
    if (!res) return false;
    if (PQntuples(res) == 0) {
        PQclear(res);
        out_profile.user_id = user_id;
        out_profile.first_name = "";
        out_profile.last_name = "";
        out_profile.bio = "";
        out_profile.birth_day = 0;
        out_profile.birth_month = 0;
        out_profile.birth_year = 0;
        out_profile.linked_channel_id = "";
        out_profile.business_hours_json = "";
        return true;
    }
    out_profile.user_id = user_id;
    out_profile.first_name = PQgetvalue(res, 0, 0);
    out_profile.last_name = PQgetvalue(res, 0, 1);
    out_profile.bio = PQgetvalue(res, 0, 2);
    out_profile.birth_day = atoi(PQgetvalue(res, 0, 3));
    out_profile.birth_month = atoi(PQgetvalue(res, 0, 4));
    out_profile.birth_year = atoi(PQgetvalue(res, 0, 5));
    out_profile.linked_channel_id = PQgetvalue(res, 0, 6);
    out_profile.business_hours_json = PQgetvalue(res, 0, 7);
    PQclear(res);
    return true;
}

bool DatabaseManager::setUserLinkedChannel(const std::string& user_id, const std::string& channel_id) {
    const char* params[2] = {user_id.c_str(), channel_id.c_str()};
    PGresult* res = db_->executePrepared("upsert_user_linked_channel", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::setUserBusinessHours(const std::string& user_id, const std::string& business_hours_json) {
    const char* params[2] = {user_id.c_str(), business_hours_json.c_str()};
    PGresult* res = db_->executePrepared("set_user_business_hours", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::upsertUserPrivacy(const std::string& user_id, const UserPrivacySettings& settings) {
    const std::string send_read_receipts = settings.send_read_receipts ? "true" : "false";
    const char* params[6] = {
        user_id.c_str(),
        settings.bio_visibility.c_str(),
        settings.birth_visibility.c_str(),
        settings.last_seen_visibility.c_str(),
        settings.avatar_visibility.c_str(),
        send_read_receipts.c_str()
    };
    PGresult* res = db_->executePrepared("upsert_user_privacy", 6, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::getUserPrivacy(const std::string& user_id, UserPrivacySettings& out_settings) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_privacy", 1, params);
    if (!res) return false;
    if (PQntuples(res) == 0) {
        PQclear(res);
        // Defaults (must match schema defaults)
        out_settings.bio_visibility = "everyone";
        out_settings.birth_visibility = "contacts";
        out_settings.last_seen_visibility = "contacts";
        out_settings.avatar_visibility = "everyone";
        out_settings.send_read_receipts = true;
        return true;
    }
    out_settings.bio_visibility = PQgetvalue(res, 0, 0);
    out_settings.birth_visibility = PQgetvalue(res, 0, 1);
    out_settings.last_seen_visibility = PQgetvalue(res, 0, 2);
    out_settings.avatar_visibility = PQgetvalue(res, 0, 3);
    out_settings.send_read_receipts = (strcmp(PQgetvalue(res, 0, 4), "t") == 0);
    PQclear(res);
    return true;
}

bool DatabaseManager::getUserPublic(const std::string& user_id, std::string& out_username, std::string& out_avatar_url) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_public", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_username = PQgetvalue(res, 0, 1);
    out_avatar_url = PQgetvalue(res, 0, 2);
    PQclear(res);
    return true;
}

int DatabaseManager::countUserChannelsV2(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("count_user_channels_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

int DatabaseManager::countUserChannelsLegacy(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("count_user_channels_legacy", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

int DatabaseManager::countUserPublicLinksV2(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("count_user_public_links_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

int DatabaseManager::countUserPublicLinksLegacy(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("count_user_public_links_legacy", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

std::vector<ChatPin> DatabaseManager::getChatPins(const std::string& user_id) {
    std::vector<ChatPin> pins;
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_pins", 1, params);
    if (!res) {
        return pins;
    }
    int rows = PQntuples(res);
    pins.reserve(rows);
    for (int i = 0; i < rows; i++) {
        ChatPin pin;
        pin.chat_type = PQgetvalue(res, i, 0);
        pin.chat_id = PQgetvalue(res, i, 1);
        pins.push_back(pin);
    }
    PQclear(res);
    return pins;
}

bool DatabaseManager::pinChat(const std::string& user_id, const std::string& chat_type, const std::string& chat_id) {
    const char* params[3] = {user_id.c_str(), chat_type.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("pin_chat", 3, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::unpinChat(const std::string& user_id, const std::string& chat_type, const std::string& chat_id) {
    const char* params[3] = {user_id.c_str(), chat_type.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("unpin_chat", 3, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

std::string DatabaseManager::getChatFoldersJson(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_folders", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return "[]";
    }
    std::string json = PQgetvalue(res, 0, 0);
    PQclear(res);
    if (json.empty()) {
        return "[]";
    }
    return json;
}

bool DatabaseManager::setChatFoldersJson(const std::string& user_id, const std::string& folders_json) {
    const char* params[2] = {user_id.c_str(), folders_json.c_str()};
    PGresult* res = db_->executePrepared("set_chat_folders", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::createFriendRequest(const std::string& sender_id, const std::string& receiver_id) {
    const char* param_values[2] = {sender_id.c_str(), receiver_id.c_str()};
    PGresult* res = db_->executePrepared("create_friend_request", 2, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::acceptFriendRequest(const std::string& request_id) {
    // Get request info
    const char* param_values[1] = {request_id.c_str()};
    PGresult* res = db_->executePrepared("get_friend_request", 1, param_values);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    
    std::string sender_id = PQgetvalue(res, 0, 1);
    std::string receiver_id = PQgetvalue(res, 0, 2);
    PQclear(res);
    
    // Update request status
    const char* status = "accepted";
    const char* update_params[2] = {status, request_id.c_str()};
    res = db_->executePrepared("update_friend_request_status", 2, update_params);
    if (!res) {
        return false;
    }
    PQclear(res);
    
    // Create friendship
    const char* friendship_params[2] = {sender_id.c_str(), receiver_id.c_str()};
    res = db_->executePrepared("create_friendship", 2, friendship_params);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::rejectFriendRequest(const std::string& request_id) {
    const char* status = "rejected";
    const char* param_values[2] = {status, request_id.c_str()};
    PGresult* res = db_->executePrepared("update_friend_request_status", 2, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::createFriendshipPair(const std::string& user_a, const std::string& user_b) {
    const char* param_values[2] = {user_a.c_str(), user_b.c_str()};
    PGresult* res = db_->executePrepared("create_friendship_pair", 2, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

std::vector<FriendRequest> DatabaseManager::getFriendRequests(const std::string& user_id) {
    std::vector<FriendRequest> requests;
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_friend_requests", 1, param_values);
    if (!res) {
        return requests;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        FriendRequest req;
        req.id = PQgetvalue(res, i, 0);
        req.sender_id = PQgetvalue(res, i, 1);
        req.receiver_id = PQgetvalue(res, i, 2);
        req.status = PQgetvalue(res, i, 3);
        req.created_at = PQgetvalue(res, i, 4);
        requests.push_back(req);
    }
    
    PQclear(res);
    return requests;
}

bool DatabaseManager::areFriends(const std::string& user1_id, const std::string& user2_id) {
    const char* param_values[2] = {user1_id.c_str(), user2_id.c_str()};
    
    PGresult* res = db_->executePrepared("are_friends", 2, param_values);
    if (!res) {
        return false;
    }
    
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count > 0;
}

bool DatabaseManager::setUserActive(const std::string& user_id, bool is_active) {
    const char* param_values[2] = {user_id.c_str(), is_active ? "true" : "false"};
    PGresult* res = db_->executePrepared("set_user_active", 2, param_values);
    if (!res) {
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::updateUserPasswordHash(const std::string& user_id, const std::string& password_hash) {
    const char* param_values[2] = {user_id.c_str(), password_hash.c_str()};
    PGresult* res = db_->executePrepared("update_user_password_hash", 2, param_values);
    if (!res) {
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::deleteMessagesByUser(const std::string& user_id) {
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("delete_messages_by_user", 1, param_values);
    if (!res) {
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

long long DatabaseManager::countUsers() {
    PGresult* res = db_->executePrepared("count_users", 0, nullptr);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countMessagesToday() {
    PGresult* res = db_->executePrepared("count_messages_today", 0, nullptr);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countOnlineUsers(int threshold_seconds) {
    const std::string threshold_str = std::to_string(threshold_seconds);
    const char* param_values[1] = {threshold_str.c_str()};
    PGresult* res = db_->executePrepared("count_online_users", 1, param_values);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

bool DatabaseManager::upsertUserSession(const std::string& token,
                                        const std::string& user_id,
                                        int ttl_seconds) {
    const std::string ttl_str = std::to_string(ttl_seconds);
    const char* params[3] = {token.c_str(), user_id.c_str(), ttl_str.c_str()};
    PGresult* res = db_->executePrepared("upsert_user_session", 3, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

std::string DatabaseManager::getUserIdBySessionToken(const std::string& token) {
    const char* params[1] = {token.c_str()};
    PGresult* res = db_->executePrepared("get_user_id_by_session", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return "";
    }
    std::string user_id = PQgetvalue(res, 0, 0);
    PQclear(res);
    // best-effort touch
    PGresult* touch = db_->executePrepared("touch_user_session", 1, params);
    if (touch) PQclear(touch);
    return user_id;
}

bool DatabaseManager::deleteUserSession(const std::string& token) {
    const char* params[1] = {token.c_str()};
    PGresult* res = db_->executePrepared("delete_user_session", 1, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

std::vector<UserSession> DatabaseManager::getUserSessions(const std::string& user_id) {
    std::vector<UserSession> sessions;
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("list_user_sessions", 1, params);
    if (!res) {
        return sessions;
    }
    int rows = PQntuples(res);
    sessions.reserve(rows);
    for (int i = 0; i < rows; i++) {
        UserSession session;
        session.token = PQgetvalue(res, i, 0);
        session.created_at = PQgetvalue(res, i, 1);
        session.last_seen = PQgetvalue(res, i, 2);
        session.user_agent = PQgetvalue(res, i, 3);
        sessions.push_back(std::move(session));
    }
    PQclear(res);
    return sessions;
}

bool DatabaseManager::updateUserSessionUserAgent(const std::string& token, const std::string& user_agent) {
    const char* params[2] = {token.c_str(), user_agent.c_str()};
    PGresult* res = db_->executePrepared("update_user_session_user_agent", 2, params);
    if (!res) {
        return false;
    }
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::upsertPushToken(const std::string& user_id,
                                      const std::string& device_token,
                                      const std::string& platform) {
    const char* params[3] = {user_id.c_str(), device_token.c_str(), platform.c_str()};
    PGresult* res = db_->executePrepared("upsert_push_token", 3, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::deletePushToken(const std::string& user_id, const std::string& device_token) {
    const char* params[2] = {user_id.c_str(), device_token.c_str()};
    PGresult* res = db_->executePrepared("delete_push_token", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

std::vector<PushTokenInfo> DatabaseManager::getPushTokensForUser(const std::string& user_id) {
    std::vector<PushTokenInfo> tokens;
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_push_tokens", 1, params);
    if (!res) return tokens;
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        const char* device = PQgetvalue(res, i, 0);
        const char* platform = PQgetvalue(res, i, 1);
        if (device && *device) {
            PushTokenInfo info;
            info.device_token = device;
            info.platform = platform ? platform : "";
            tokens.push_back(std::move(info));
        }
    }
    PQclear(res);
    return tokens;
}

bool DatabaseManager::createChannelV2(const std::string& title,
                                      const std::string& creator_id,
                                      std::string& chat_id,
                                      bool is_public,
                                      const std::string& username,
                                      const std::string& about,
                                      int slow_mode_sec,
                                      bool sign_messages) {
    PGresult* res = nullptr;
    auto rollback = [&]() {
        PGresult* rb = db_->executeQuery("ROLLBACK");
        if (rb) PQclear(rb);
    };

    res = db_->executeQuery("BEGIN");
    if (!res) {
        return false;
    }
    PQclear(res);

    const std::string type = "channel";
    const std::string slow = std::to_string(slow_mode_sec);
    const char* params[8] = {
        type.c_str(),
        title.c_str(),
        username.c_str(),
        about.c_str(),
        is_public ? "true" : "false",
        slow.c_str(),
        sign_messages ? "true" : "false",
        creator_id.c_str()
    };

    res = db_->executePrepared("create_chat_v2", 8, params);
    if (!res || PQntuples(res) == 0 || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        rollback();
        return false;
    }
    chat_id = PQgetvalue(res, 0, 0);
    PQclear(res);

    const char* seqParams[1] = {chat_id.c_str()};
    res = db_->executePrepared("insert_chat_sequence_v2", 1, seqParams);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        rollback();
        return false;
    }
    PQclear(res);

    const char* ownerParams[2] = {chat_id.c_str(), creator_id.c_str()};
    res = db_->executePrepared("insert_chat_participant_owner_v2", 2, ownerParams);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        rollback();
        return false;
    }
    PQclear(res);

    // Create Owner role with all permissions and assign
    const std::string permsAll = std::to_string(kAllAdminPerms);
    const char* roleParams[3] = {chat_id.c_str(), "Owner", permsAll.c_str()};
    res = db_->executePrepared("upsert_admin_role_v2", 3, roleParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        rollback();
        return false;
    }
    std::string role_id = PQgetvalue(res, 0, 0);
    PQclear(res);

    const char* assignParams[3] = {chat_id.c_str(), role_id.c_str(), creator_id.c_str()};
    res = db_->executePrepared("set_owner_adminrole_v2", 3, assignParams);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        rollback();
        return false;
    }
    PQclear(res);

    res = db_->executeQuery("COMMIT");
    if (!res) {
        rollback();
        return false;
    }
    PQclear(res);
    return true;
}

bool DatabaseManager::linkChat(const std::string& channel_id, const std::string& discussion_id) {
    const char* chParams[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_type_v2", 1, chParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    std::string channelType = PQgetvalue(res, 0, 0);
    PQclear(res);

    const char* discParams[1] = {discussion_id.c_str()};
    res = db_->executePrepared("get_chat_type_v2", 1, discParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    std::string discussionType = PQgetvalue(res, 0, 0);
    PQclear(res);

    if (channelType != "channel" || (discussionType != "supergroup" && discussionType != "group")) {
        return false;
    }

    PGresult* beginRes = db_->executeQuery("BEGIN");
    if (!beginRes) {
        return false;
    }
    PQclear(beginRes);

    const char* linkParams[2] = {channel_id.c_str(), discussion_id.c_str()};
    res = db_->executePrepared("insert_chat_link_v2", 2, linkParams);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        PGresult* rb = db_->executeQuery("ROLLBACK");
        if (rb) PQclear(rb);
        return false;
    }
    PQclear(res);

    PGresult* commitRes = db_->executeQuery("COMMIT");
    if (!commitRes) {
        PGresult* rb = db_->executeQuery("ROLLBACK");
        if (rb) PQclear(rb);
        return false;
    }
    PQclear(commitRes);
    return true;
}

bool DatabaseManager::promoteAdmin(const std::string& chat_id,
                                   const std::string& user_id,
                                   uint64_t perms,
                                   const std::string& title) {
    PGresult* res = db_->executeQuery("BEGIN");
    if (!res) {
        return false;
    }
    PQclear(res);

    const std::string permsStr = std::to_string(perms);
    const char* roleParams[3] = {chat_id.c_str(), title.c_str(), permsStr.c_str()};
    res = db_->executePrepared("upsert_admin_role_v2", 3, roleParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        PGresult* rb = db_->executeQuery("ROLLBACK");
        if (rb) PQclear(rb);
        return false;
    }
    std::string role_id = PQgetvalue(res, 0, 0);
    PQclear(res);

    const char* participantParams[3] = {chat_id.c_str(), user_id.c_str(), role_id.c_str()};
    res = db_->executePrepared("upsert_participant_admin_v2", 3, participantParams);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        PGresult* rb = db_->executeQuery("ROLLBACK");
        if (rb) PQclear(rb);
        return false;
    }
    PQclear(res);

    res = db_->executeQuery("COMMIT");
    if (!res) {
        PGresult* rb = db_->executeQuery("ROLLBACK");
        if (rb) PQclear(rb);
        return false;
    }
    PQclear(res);
    return true;
}

ChatPermissions DatabaseManager::getChatPermissions(const std::string& chat_id, const std::string& user_id) {
    ChatPermissions permissions;
    Chat chat_meta;
    std::string resolved_chat_id = chat_id;

    auto ensureChatLoaded = [&]() {
        if (!chat_meta.id.empty()) {
            return;
        }
        // First, try by direct chat id
        chat_meta = getChatV2(resolved_chat_id);
        if (!chat_meta.id.empty()) {
            resolved_chat_id = chat_meta.id;
            return;
        }
        // If the caller passed @username or slug, resolve to chat id
        std::string username_key = resolved_chat_id;
        if (!username_key.empty() && username_key[0] == '@') {
            username_key = username_key.substr(1);
        }
        if (!username_key.empty()) {
            chat_meta = getChatByUsernameV2(username_key);
            if (!chat_meta.id.empty()) {
                resolved_chat_id = chat_meta.id;
            }
        }
    };

    auto applyOwnerOverride = [&]() {
        ensureChatLoaded();
        if (!chat_meta.id.empty() && chat_meta.created_by == user_id) {
            permissions.found = true;
            permissions.status = "owner";
            permissions.perms = kAllAdminPerms;
            return true;
        }
        return false;
    };

    ensureChatLoaded();
    if (chat_meta.id.empty()) {
        // Unknown chat; return default (not found)
        return permissions;
    }

    const char* params[2] = {resolved_chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("get_participant_role_v2", 2, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        // If the participant row is missing, fall back to creator ownership
        applyOwnerOverride();
        return permissions;
    }

    permissions.found = true;
    permissions.status = PQgetvalue(res, 0, 0);
    bool hasRoleId = !PQgetisnull(res, 0, 1);
    std::string roleId = hasRoleId ? PQgetvalue(res, 0, 1) : "";
    PQclear(res);
    res = nullptr;

    if (permissions.status == "owner") {
        permissions.perms = kAllAdminPerms;
        return permissions;
    }
    // Creator override even if participant row exists but lacks admin flags
    if (applyOwnerOverride()) {
        return permissions;
    }
    if (permissions.status == "admin" && hasRoleId) {
        const char* roleParams[1] = {roleId.c_str()};
        res = db_->executePrepared("get_admin_role_perms_v2", 1, roleParams);
        if (res && PQntuples(res) > 0) {
            permissions.perms = static_cast<uint64_t>(atoll(PQgetvalue(res, 0, 0)));
        }
        if (res) PQclear(res);
    }
    // As a final safeguard, ensure creator always retains full permissions
    applyOwnerOverride();
    return permissions;
}

Restriction DatabaseManager::getParticipantRestriction(const std::string& chat_id, const std::string& user_id) {
    Restriction r;
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("get_participant_restriction_v2", 2, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return r;
    }
    r.found = true;
    r.can_send_messages = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
    r.can_send_media    = (strcmp(PQgetvalue(res, 0, 1), "t") == 0);
    r.can_add_links     = (strcmp(PQgetvalue(res, 0, 2), "t") == 0);
    r.can_invite_users  = (strcmp(PQgetvalue(res, 0, 3), "t") == 0);
    if (!PQgetisnull(res, 0, 4)) {
        r.until = PQgetvalue(res, 0, 4);
    }
    PQclear(res);
    return r;
}

Chat DatabaseManager::getChatV2(const std::string& chat_id) {
    Chat chat;
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return chat;
    }
    chat.id = PQgetvalue(res, 0, 0);
    chat.type = PQgetvalue(res, 0, 1);
    chat.title = PQgetvalue(res, 0, 2);
    chat.username = PQgetvalue(res, 0, 3);
    chat.about = PQgetvalue(res, 0, 4);
    chat.avatar_url = PQgetvalue(res, 0, 5);
    chat.is_public = (strcmp(PQgetvalue(res, 0, 6), "t") == 0);
    chat.sign_messages = (strcmp(PQgetvalue(res, 0, 7), "t") == 0);
    chat.slow_mode_sec = atoi(PQgetvalue(res, 0, 8));
    chat.created_by = PQgetvalue(res, 0, 9);
    chat.created_at = PQgetvalue(res, 0, 10);
    chat.is_verified = (strcmp(PQgetvalue(res, 0, 11), "t") == 0);
    PQclear(res);
    return chat;
}

Chat DatabaseManager::getChatByUsernameV2(const std::string& username) {
    Chat chat;
    const char* params[1] = {username.c_str()};
    PGresult* res = db_->executePrepared("get_chat_by_username_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return chat;
    }
    chat.id = PQgetvalue(res, 0, 0);
    chat.type = PQgetvalue(res, 0, 1);
    chat.title = PQgetvalue(res, 0, 2);
    chat.username = PQgetvalue(res, 0, 3);
    chat.about = PQgetvalue(res, 0, 4);
    chat.avatar_url = PQgetvalue(res, 0, 5);
    chat.is_public = PQgetvalue(res, 0, 6)[0] == 't';
    chat.sign_messages = PQgetvalue(res, 0, 7)[0] == 't';
    chat.slow_mode_sec = atoi(PQgetvalue(res, 0, 8));
    chat.created_by = PQgetvalue(res, 0, 9);
    chat.created_at = PQgetvalue(res, 0, 10);
    chat.is_verified = PQgetvalue(res, 0, 11)[0] == 't';
    PQclear(res);
    return chat;
}

bool DatabaseManager::sendChannelMessageWithDiscussion(const std::string& channel_id,
                                                       const std::string& sender_id,
                                                       const std::string& content_json,
                                                       const std::string& content_type,
                                                       const std::string& replied_to_global_id,
                                                       bool is_silent,
                                                       std::string* out_channel_message_id,
                                                       std::string* out_discussion_message_id) {
    // Start transaction
    PGresult* res = db_->executeQuery("BEGIN");
    if (!res) {
        return false;
    }
    PQclear(res);

    Chat chat = getChatV2(channel_id);
    if (chat.id.empty() || chat.type != "channel") {
        db_->executeQuery("ROLLBACK");
        return false;
    }

    // Ensure sequence row exists
    const char* seqParams[1] = {channel_id.c_str()};
    res = db_->executePrepared("insert_chat_sequence_v2", 1, seqParams);
    if (res) PQclear(res);

    // Allocate local id
    res = db_->executePrepared("inc_local_id_v2", 1, seqParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        db_->executeQuery("ROLLBACK");
        return false;
    }
    std::string channel_local_id = PQgetvalue(res, 0, 0);
    PQclear(res);

    // Sender handling (sign_messages -> sender_id null)
    const char* senderParam = chat.sign_messages ? nullptr : sender_id.c_str();
    const char* replyParam = replied_to_global_id.empty() ? nullptr : replied_to_global_id.c_str();
    const char* threadParam = nullptr;

    const char* msgParams[8] = {
        channel_id.c_str(),
        channel_local_id.c_str(),
        senderParam,
        replyParam,
        threadParam,
        content_type.c_str(),
        content_json.c_str(),
        is_silent ? "true" : "false"
    };
    res = db_->executePrepared("insert_chat_message_v2", 8, msgParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        db_->executeQuery("ROLLBACK");
        return false;
    }
    std::string channel_msg_id = PQgetvalue(res, 0, 0);
    PQclear(res);

    // Check for discussion link
    const char* linkParams[1] = {channel_id.c_str()};
    res = db_->executePrepared("get_chat_link_v2", 1, linkParams);
    std::string discussion_id;
    if (res && PQntuples(res) > 0 && !PQgetisnull(res, 0, 0)) {
        discussion_id = PQgetvalue(res, 0, 0);
    }
    if (res) PQclear(res);

    if (!discussion_id.empty()) {
        // ensure sequence for discussion
        const char* seq2[1] = {discussion_id.c_str()};
        res = db_->executePrepared("insert_chat_sequence_v2", 1, seq2);
        if (res) PQclear(res);

        res = db_->executePrepared("inc_local_id_v2", 1, seq2);
        if (!res || PQntuples(res) == 0) {
            if (res) PQclear(res);
            db_->executeQuery("ROLLBACK");
            return false;
        }
        std::string discussion_local_id = PQgetvalue(res, 0, 0);
        PQclear(res);

        // Shadow content referencing channel message
        std::string shadow = "{\"shadow_of\":" + channel_msg_id + ",\"preview\":" + content_json + "}";
        const char* shadowSender = nullptr; // posted as channel
        const char* threadRoot = channel_msg_id.c_str();
        const char* replyNull = nullptr;
        const char* shadowParams[8] = {
            discussion_id.c_str(),
            discussion_local_id.c_str(),
            shadowSender,
            replyNull,
            threadRoot,
            "shadow",
            shadow.c_str(),
            "false"
        };
        res = db_->executePrepared("insert_chat_message_v2", 8, shadowParams);
        if (!res || PQntuples(res) == 0) {
            if (res) PQclear(res);
            db_->executeQuery("ROLLBACK");
            return false;
        }
        if (out_discussion_message_id) {
            *out_discussion_message_id = PQgetvalue(res, 0, 0);
        }
        PQclear(res);
    }

    res = db_->executeQuery("COMMIT");
    if (!res) {
        db_->executeQuery("ROLLBACK");
        return false;
    }
    PQclear(res);
    if (out_channel_message_id) {
        *out_channel_message_id = channel_msg_id;
    }
    return true;
}

bool DatabaseManager::addMessageReactionV2(const std::string& message_id,
                                           const std::string& user_id,
                                           const std::string& reaction) {
    long long msgId = 0;
    try { msgId = std::stoll(message_id); } catch (...) { return false; }
    const int shard = static_cast<int>((std::hash<std::string>{}(user_id) % 16));

    PGresult* res = db_->executeQuery("BEGIN");
    if (!res) return false;
    PQclear(res);

    const char* cntParams[1] = {message_id.c_str()};
    res = db_->executePrepared("insert_reaction_counts_v2", 1, cntParams);
    if (res) PQclear(res);

    const std::string shardStr = std::to_string(shard);
    const char* insParams[4] = {shardStr.c_str(), message_id.c_str(), user_id.c_str(), reaction.c_str()};
    res = db_->executePrepared("insert_reaction_shard_v2", 4, insParams);
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    PQclear(res);

    const char* updParams[3] = {message_id.c_str(), reaction.c_str(), "1"};
    res = db_->executePrepared("update_reaction_count_v2", 3, updParams);
    if (!res || PQntuples(res) == 0) { if (res) PQclear(res); db_->executeQuery("ROLLBACK"); return false; }
    PQclear(res);

    res = db_->executeQuery("COMMIT");
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    PQclear(res);
    return true;
}

bool DatabaseManager::removeMessageReactionV2(const std::string& message_id,
                                              const std::string& user_id,
                                              const std::string& reaction) {
    long long msgId = 0;
    try { msgId = std::stoll(message_id); } catch (...) { return false; }
    const int shard = static_cast<int>((std::hash<std::string>{}(user_id) % 16));

    PGresult* res = db_->executeQuery("BEGIN");
    if (!res) return false;
    PQclear(res);

    const std::string shardStr = std::to_string(shard);
    const char* delParams[4] = {shardStr.c_str(), message_id.c_str(), user_id.c_str(), reaction.c_str()};
    res = db_->executePrepared("delete_reaction_shard_v2", 4, delParams);
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    bool deleted = (PQcmdTuples(res) && std::string(PQcmdTuples(res)) != "0");
    PQclear(res);

    if (deleted) {
        const char* updParams[3] = {message_id.c_str(), reaction.c_str(), "-1"};
        res = db_->executePrepared("update_reaction_count_v2", 3, updParams);
        if (!res || PQntuples(res) == 0) { if (res) PQclear(res); db_->executeQuery("ROLLBACK"); return false; }
        PQclear(res);
    }

    res = db_->executeQuery("COMMIT");
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    PQclear(res);
    return deleted;
}

std::vector<ReactionCount> DatabaseManager::getMessageReactionCountsV2(const std::string& message_id) {
    std::vector<ReactionCount> result;
    const char* params[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("get_reaction_counts_kv_v2", 1, params);
    if (!res) {
        return result;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        ReactionCount rc;
        rc.reaction = PQgetvalue(res, i, 0);
        rc.count = atoll(PQgetvalue(res, i, 1));
        result.push_back(rc);
    }
    PQclear(res);
    return result;
}

std::vector<DatabaseManager::ChannelMessage> DatabaseManager::getChannelMessagesV2(const std::string& chat_id, long long offset_local, int limit) {
    std::vector<DatabaseManager::ChannelMessage> messages;
    const std::string limitStr = std::to_string(limit);
    const std::string offsetStr = std::to_string(offset_local);
    const char* params[3] = {chat_id.c_str(), offsetStr.c_str(), limitStr.c_str()};
    PGresult* res = db_->executePrepared("get_chat_messages_v2", 3, params);
    if (!res) {
        return messages;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        DatabaseManager::ChannelMessage m;
        m.id = PQgetvalue(res, i, 0);
        m.local_id = atoll(PQgetvalue(res, i, 1));
        m.channel_id = chat_id;
        m.sender_id = PQgetvalue(res, i, 2) ? PQgetvalue(res, i, 2) : "";
        m.sender_username = PQgetvalue(res, i, 3) ? PQgetvalue(res, i, 3) : "";
        std::string contentJson = PQgetvalue(res, i, 4);
        m.message_type = PQgetvalue(res, i, 5);
        m.is_silent = PQgetvalue(res, i, 6)[0] == 't';
        m.created_at = PQgetvalue(res, i, 7);
        m.content_json = contentJson;
        // Extract human text from JSONB content when possible
        std::string extracted = contentJson;
        if (!contentJson.empty() && contentJson.front() == '{') {
            auto kv = JsonParser::parse(contentJson);
            auto it = kv.find("text");
            if (it != kv.end()) extracted = it->second;
        }
        // Remove optional surrounding quotes (jsonb string form)
        if (extracted.size() >= 2 && extracted.front() == '"' && extracted.back() == '"') {
            extracted = extracted.substr(1, extracted.size() - 2);
        }
        m.content = extracted;
        m.file_path = "";
        m.file_name = "";
        m.file_size = 0;
        m.is_pinned = PQgetvalue(res, i, 8)[0] == 't';
        m.views_count = 0;
        messages.push_back(m);
    }
    PQclear(res);
    return messages;
}

bool DatabaseManager::pinChatMessageV2(const std::string& chat_id, const std::string& message_id, const std::string& pinned_by) {
    const char* params[3] = {chat_id.c_str(), message_id.c_str(), pinned_by.c_str()};
    PGresult* res = db_->executePrepared("pin_chat_message_v2", 3, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::unpinChatMessageV2(const std::string& chat_id) {
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("unpin_chat_message_v2", 1, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::getChatMessageMetaV2(const std::string& message_id, std::string& out_chat_id, std::string& out_sender_id) {
    const char* params[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_message_meta_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_chat_id = PQgetvalue(res, 0, 0);
    out_sender_id = PQgetvalue(res, 0, 1);
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteChatMessageV2(const std::string& chat_id, const std::string& message_id) {
    const char* params[2] = {message_id.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("delete_chat_message_v2", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK) && std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::upsertChannelReadState(const std::string& chat_id,
                                             const std::string& user_id,
                                             long long max_read_local) {
    const std::string maxReadStr = std::to_string(max_read_local);
    const char* params[3] = {chat_id.c_str(), user_id.c_str(), maxReadStr.c_str()};
    PGresult* res = db_->executePrepared("upsert_channel_read_state_v2", 3, params);
    if (!res) {
        return false;
    }
    PQclear(res);
    return true;
}

long long DatabaseManager::getChannelUnreadCount(const std::string& chat_id,
                                                 const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_unread_v2", 2, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long unread = atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    if (unread < 0) unread = 0;
    return unread;
}

bool DatabaseManager::createChannelInviteLink(const std::string& channel_id,
                                              const std::string& creator_id,
                                              int expire_seconds,
                                              int usage_limit,
                                              std::string& out_token) {
    const std::string expireStr = std::to_string(expire_seconds);
    const std::string usageStr = std::to_string(usage_limit);
    const char* params[4] = {channel_id.c_str(), creator_id.c_str(), expireStr.c_str(), usageStr.c_str()};
    Chat chat = getChatV2(channel_id);
    const char* stmt = (!chat.id.empty() && chat.type == "channel") ? "create_chat_invite_v2" : "create_channel_invite_legacy";
    PGresult* res = db_->executePrepared(stmt, 4, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_token = PQgetvalue(res, 0, 0);
    PQclear(res);
    return true;
}

bool DatabaseManager::revokeChannelInviteLink(const std::string& token) {
    const char* params[1] = {token.c_str()};
    PGresult* res = db_->executePrepared("revoke_chat_invite_v2", 1, params);
    if (res) {
        bool ok = std::string(PQcmdTuples(res)) != "0";
        PQclear(res);
        if (ok) return true;
    }
    res = db_->executePrepared("revoke_channel_invite_legacy", 1, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::joinChannelByInviteToken(const std::string& token,
                                               const std::string& user_id,
                                               std::string& out_channel_id,
                                               bool auto_join,
                                               bool* out_is_v2) {
    const char* params[1] = {token.c_str()};
    PGresult* res = db_->executePrepared("get_chat_invite_v2", 1, params);
    bool is_v2_invite = false;
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        res = db_->executePrepared("get_channel_invite_legacy", 1, params);
        if (!res || PQntuples(res) == 0) {
            if (res) PQclear(res);
            return false;
        }
    } else {
        is_v2_invite = true;
    }
    std::string channel_id = PQgetvalue(res, 0, 0);
    bool is_revoked = PQgetvalue(res, 0, 4)[0] == 't';
    std::string expire_at = PQgetisnull(res, 0, 1) ? "" : PQgetvalue(res, 0, 1);
    long long usage_limit = PQgetisnull(res, 0, 2) ? 0 : atoll(PQgetvalue(res, 0, 2));
    long long usage_count = PQgetisnull(res, 0, 3) ? 0 : atoll(PQgetvalue(res, 0, 3));
    PQclear(res);

    if (out_is_v2) {
        *out_is_v2 = is_v2_invite;
    }
    if (is_revoked) return false;
    if (!expire_at.empty()) {
        PGresult* nowRes = db_->executeQuery("SELECT now() < '" + expire_at + "'::timestamptz");
        if (!nowRes || PQntuples(nowRes) == 0 || PQgetvalue(nowRes, 0, 0)[0] != 't') {
            if (nowRes) PQclear(nowRes);
            return false;
        }
        PQclear(nowRes);
    }
    if (usage_limit > 0 && usage_count >= usage_limit) {
        return false;
    }

    const char* incStmt = is_v2_invite ? "inc_chat_invite_usage_v2" : "inc_channel_invite_usage_legacy";
    PGresult* inc = db_->executePrepared(incStmt, 1, params);
    if (inc) PQclear(inc);

    if (!auto_join) {
        out_channel_id = channel_id;
        return true;
    }

    bool added = false;
    if (is_v2_invite) {
        const char* memberParams[2] = {channel_id.c_str(), user_id.c_str()};
        PGresult* addRes = db_->executePrepared("upsert_chat_member_v2", 2, memberParams);
        added = (addRes != nullptr);
        if (addRes) PQclear(addRes);
    } else {
        added = addChannelMember(channel_id, user_id, "subscriber");
    }

    if (added) {
        out_channel_id = channel_id;
    }
    return added;
}

std::vector<std::string> DatabaseManager::getChannelSubscriberIds(const std::string& chat_id) {
    std::vector<std::string> ids;
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_subscribers_v2", 1, params);
    if (!res) return ids;
    int rows = PQntuples(res);
    ids.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        ids.emplace_back(PQgetvalue(res, i, 0));
    }
    PQclear(res);
    return ids;
}

int DatabaseManager::countChannelSubscribersV2(const std::string& chat_id) {
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("count_chat_subscribers_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

int DatabaseManager::countChannelMembersV2(const std::string& chat_id) {
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("count_chat_members_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

std::string DatabaseManager::getChannelIdByInviteToken(const std::string& token) {
    const char* params[1] = {token.c_str()};
    PGresult* res = db_->executePrepared("get_chat_invite_v2", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        res = db_->executePrepared("get_channel_invite_legacy", 1, params);
        if (!res || PQntuples(res) == 0) {
            if (res) PQclear(res);
            return "";
        }
    }
    std::string channel_id = PQgetvalue(res, 0, 0);
    PQclear(res);
    return channel_id;
}

bool DatabaseManager::updateChatTitleV2(const std::string& chat_id, const std::string& title) {
    const char* params[2] = {title.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("update_chat_title_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::updateChatAboutV2(const std::string& chat_id, const std::string& about) {
    const char* params[2] = {about.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("update_chat_about_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::updateChatUsernameV2(const std::string& chat_id, const std::string& username) {
    const char* params[2] = {username.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("update_chat_username_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::updateChatIsPublicV2(const std::string& chat_id, bool is_public) {
    const char* params[2] = {is_public ? "true" : "false", chat_id.c_str()};
    PGresult* res = db_->executePrepared("update_chat_is_public_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::updateChatSignMessagesV2(const std::string& chat_id, bool sign_messages) {
    const char* params[2] = {sign_messages ? "true" : "false", chat_id.c_str()};
    PGresult* res = db_->executePrepared("update_chat_sign_messages_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::updateChatAvatarV2(const std::string& chat_id, const std::string& avatar_url) {
    const char* params[2] = {avatar_url.c_str(), chat_id.c_str()};
    PGresult* res = db_->executePrepared("update_chat_avatar_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::upsertChatMemberV2(const std::string& chat_id, const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("upsert_chat_member_v2", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::leaveChatV2(const std::string& chat_id, const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("leave_chat_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::demoteChatAdminV2(const std::string& chat_id, const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("demote_chat_admin_v2", 2, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

ChatParticipant DatabaseManager::getChatParticipantV2(const std::string& chat_id, const std::string& user_id) {
    ChatParticipant participant;
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_participant_v2", 2, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return participant;
    }
    participant.chat_id = chat_id;
    participant.user_id = user_id;
    participant.status = PQgetvalue(res, 0, 0);
    participant.admin_role_id = PQgetisnull(res, 0, 1) ? "" : PQgetvalue(res, 0, 1);
    participant.joined_at = PQgetvalue(res, 0, 2);
    PQclear(res);
    return participant;
}

bool DatabaseManager::updateChatParticipantStatusV2(const std::string& chat_id, const std::string& user_id, const std::string& status) {
    const char* params[3] = {chat_id.c_str(), user_id.c_str(), status.c_str()};
    PGresult* res = db_->executePrepared("update_chat_participant_status_v2", 3, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

std::vector<DatabaseManager::Channel> DatabaseManager::getUserChannelsV2(const std::string& user_id) {
    std::vector<Channel> channels;
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_channels_v2", 1, params);
    if (!res) return channels;
    int rows = PQntuples(res);
    channels.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        Channel c;
        c.id = PQgetvalue(res, i, 0);
        c.name = PQgetvalue(res, i, 1);
        c.description = PQgetvalue(res, i, 2);
        c.custom_link = PQgetvalue(res, i, 3);
        c.avatar_url = PQgetvalue(res, i, 4);
        bool is_public = (PQgetvalue(res, i, 5)[0] == 't');
        c.is_private = !is_public;
        // In v2, sign_messages=true means "do NOT show author" (Telegram-like); keep legacy show_author inverse.
        bool sign_messages = (PQgetvalue(res, i, 6)[0] == 't');
        c.show_author = !sign_messages;
        c.creator_id = PQgetvalue(res, i, 7);
        c.created_at = PQgetvalue(res, i, 8);
        c.is_verified = (PQgetvalue(res, i, 9)[0] == 't');
        channels.push_back(std::move(c));
    }
    PQclear(res);
    return channels;
}

std::vector<DatabaseManager::ChannelMember> DatabaseManager::getChannelMembersV2(const std::string& channel_id) {
    std::vector<ChannelMember> members;
    const char* params[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_members_v2", 1, params);
    if (!res) return members;
    int rows = PQntuples(res);
    members.reserve(rows);
    for (int i = 0; i < rows; ++i) {
        ChannelMember m;
        m.channel_id = channel_id;
        m.user_id = PQgetvalue(res, i, 0);
        m.username = PQgetvalue(res, i, 1);
        std::string status = PQgetvalue(res, i, 2);
        if (status == "owner") {
            m.role = "owner";
        } else if (status == "admin") {
            m.role = "admin";
        } else {
            m.role = "subscriber";
        }
        m.is_banned = (status == "kicked");
        m.joined_at = PQgetvalue(res, i, 3);
        m.admin_perms = static_cast<uint64_t>(atoll(PQgetvalue(res, i, 4)));
        m.admin_title = PQgetvalue(res, i, 5);
        if (status == "owner") {
            m.admin_perms = kAllAdminPerms;
        }
        members.push_back(std::move(m));
    }
    PQclear(res);
    return members;
}

bool DatabaseManager::deleteChatV2(const std::string& chat_id) {
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("delete_chat_v2", 1, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::createChatJoinRequest(const std::string& chat_id, const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("create_chat_join_request", 2, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::acceptChatJoinRequest(const std::string& chat_id, const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("accept_chat_join_request", 2, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    if (!success) return false;
    return upsertChatMemberV2(chat_id, user_id);
}

bool DatabaseManager::rejectChatJoinRequest(const std::string& chat_id, const std::string& user_id) {
    const char* params[2] = {chat_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("reject_chat_join_request", 2, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::vector<DatabaseManager::ChannelMember> DatabaseManager::getChatJoinRequests(const std::string& chat_id) {
    std::vector<ChannelMember> requests;
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_join_requests", 1, params);
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        requests.reserve(rows);
        for (int i = 0; i < rows; ++i) {
            ChannelMember member;
            member.id = PQgetvalue(res, i, 0);
            member.channel_id = PQgetvalue(res, i, 1);
            member.user_id = PQgetvalue(res, i, 2);
            member.username = PQgetvalue(res, i, 3);
            member.joined_at = PQgetvalue(res, i, 4);
            requests.push_back(std::move(member));
        }
    }
    if (res) PQclear(res);
    return requests;
}

bool DatabaseManager::addChatAllowedReaction(const std::string& chat_id, const std::string& reaction) {
    const char* params[2] = {chat_id.c_str(), reaction.c_str()};
    PGresult* res = db_->executePrepared("add_chat_allowed_reaction", 2, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::removeChatAllowedReaction(const std::string& chat_id, const std::string& reaction) {
    const char* params[2] = {chat_id.c_str(), reaction.c_str()};
    PGresult* res = db_->executePrepared("remove_chat_allowed_reaction", 2, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::vector<std::string> DatabaseManager::getChatAllowedReactions(const std::string& chat_id) {
    std::vector<std::string> reactions;
    const char* params[1] = {chat_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_allowed_reactions", 1, params);
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        reactions.reserve(rows);
        for (int i = 0; i < rows; ++i) {
            reactions.emplace_back(PQgetvalue(res, i, 0));
        }
    }
    if (res) PQclear(res);
    return reactions;
}

bool DatabaseManager::createPollV2(const std::string& message_id,
                                   const std::string& message_type,
                                   const std::string& question,
                                   bool is_anonymous,
                                   bool allows_multiple,
                                   const std::vector<std::string>& options,
                                   const std::string& closes_at_iso) {
    if (options.empty()) return false;
    PGresult* res = db_->executeQuery("BEGIN");
    if (!res) return false;
    PQclear(res);

    const char* pollParams[6] = {message_id.c_str(), message_type.c_str(), question.c_str(),
                                 is_anonymous ? "true" : "false",
                                 allows_multiple ? "true" : "false",
                                 closes_at_iso.c_str()};
    res = db_->executePrepared("insert_poll_v2", 6, pollParams);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        db_->executeQuery("ROLLBACK");
        return false;
    }
    std::string poll_id = PQgetvalue(res, 0, 0);
    PQclear(res);

    for (const auto& opt : options) {
        const char* optParams[2] = {poll_id.c_str(), opt.c_str()};
        res = db_->executePrepared("insert_poll_option_v2", 2, optParams);
        if (!res) { db_->executeQuery("ROLLBACK"); return false; }
        PQclear(res);
    }

    res = db_->executeQuery("COMMIT");
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    PQclear(res);
    return true;
}

bool DatabaseManager::votePollV2(const std::string& poll_id,
                                 const std::string& option_id,
                                 const std::string& user_id) {
    PGresult* res = db_->executeQuery("BEGIN");
    if (!res) return false;
    PQclear(res);

    const char* voteParams[3] = {poll_id.c_str(), option_id.c_str(), user_id.c_str()};
    res = db_->executePrepared("vote_poll_v2", 3, voteParams);
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    bool inserted = (PQcmdTuples(res) && std::string(PQcmdTuples(res)) != "0");
    PQclear(res);

    if (inserted) {
        const char* incParams[1] = {option_id.c_str()};
        res = db_->executePrepared("inc_poll_option_v2", 1, incParams);
        if (!res || PQntuples(res) == 0) { if (res) PQclear(res); db_->executeQuery("ROLLBACK"); return false; }
        PQclear(res);
    }

    res = db_->executeQuery("COMMIT");
    if (!res) { db_->executeQuery("ROLLBACK"); return false; }
    PQclear(res);
    return true;
}

DatabaseManager::Poll DatabaseManager::getPollByMessage(const std::string& message_type, const std::string& message_id) {
    Poll poll;
    const char* params[2] = {message_type.c_str(), message_id.c_str()};
    PGresult* res = db_->executePrepared("get_poll_by_message", 2, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return poll;  // Return empty poll
    }

    // First row contains poll info
    poll.id = PQgetvalue(res, 0, 0) ? PQgetvalue(res, 0, 0) : "";
    poll.message_id = PQgetvalue(res, 0, 1) ? PQgetvalue(res, 0, 1) : "";
    poll.message_type = PQgetvalue(res, 0, 2) ? PQgetvalue(res, 0, 2) : "";
    poll.question = PQgetvalue(res, 0, 3) ? PQgetvalue(res, 0, 3) : "";
    poll.is_anonymous = (PQgetvalue(res, 0, 4) && std::string(PQgetvalue(res, 0, 4)) == "t");
    poll.allows_multiple = (PQgetvalue(res, 0, 5) && std::string(PQgetvalue(res, 0, 5)) == "t");
    poll.closes_at = PQgetvalue(res, 0, 6) ? PQgetvalue(res, 0, 6) : "";

    // All rows contain options
    for (int i = 0; i < PQntuples(res); i++) {
        PollOption opt;
        opt.id = PQgetvalue(res, i, 7) ? PQgetvalue(res, i, 7) : "";
        opt.option_text = PQgetvalue(res, i, 8) ? PQgetvalue(res, i, 8) : "";
        opt.vote_count = PQgetvalue(res, i, 9) ? std::stoll(PQgetvalue(res, i, 9)) : 0;
        poll.options.push_back(opt);
    }

    PQclear(res);
    return poll;
}

} // namespace xipher
