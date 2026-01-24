#include "../include/bots/bot_scheduler.hpp"
#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"

#include <chrono>
#include <cstdlib>

namespace xipher {

BotScheduler::~BotScheduler() {
    stop();
}

void BotScheduler::start() {
    if (running_.exchange(true)) return;

    worker_ = std::thread([this]() {
        // Create a dedicated DB connection for the scheduler thread
        const char* env_host = std::getenv("XIPHER_DB_HOST");
        const char* env_port = std::getenv("XIPHER_DB_PORT");
        const char* env_name = std::getenv("XIPHER_DB_NAME");
        const char* env_user = std::getenv("XIPHER_DB_USER");
        const char* env_pass = std::getenv("XIPHER_DB_PASSWORD");

        std::string db_host = env_host ? env_host : "localhost";
        std::string db_port = env_port ? env_port : "5432";
        std::string db_name = env_name ? env_name : "xipher";
        std::string db_user = env_user ? env_user : "xipher";
        std::string db_pass = env_pass ? env_pass : "xipher";

        DatabaseManager db(db_host, db_port, db_name, db_user, db_pass);
        if (!db.initialize()) {
            Logger::getInstance().error("BotScheduler: failed to initialize DB, scheduler disabled");
            running_ = false;
            return;
        }

        Logger::getInstance().info("BotScheduler started");

        while (running_) {
            try {
                auto due = db.claimDueBotReminders(50);
                for (const auto& r : due) {
                    if (r.bot_user_id.empty() || r.scope_type.empty() || r.scope_id.empty() || r.text.empty()) continue;
                    if (r.scope_type == "direct") {
                        const std::string target = r.target_user_id.empty() ? r.scope_id : r.target_user_id;
                        db.sendMessage(r.bot_user_id, target, "⏰ Напоминание: " + r.text);
                    } else if (r.scope_type == "group") {
                        db.sendGroupMessage(r.scope_id, r.bot_user_id, "⏰ Напоминание: " + r.text);
                    }
                }
            } catch (const std::exception& e) {
                Logger::getInstance().warning(std::string("BotScheduler loop error: ") + e.what());
            } catch (...) {
                Logger::getInstance().warning("BotScheduler loop error: unknown");
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        Logger::getInstance().info("BotScheduler stopped");
    });
}

void BotScheduler::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

} // namespace xipher


