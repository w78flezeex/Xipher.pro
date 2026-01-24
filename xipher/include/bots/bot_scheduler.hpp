#ifndef XIPHER_BOT_SCHEDULER_HPP
#define XIPHER_BOT_SCHEDULER_HPP

#include <atomic>
#include <thread>
#include <string>

namespace xipher {

// Background scheduler for lightweight bots (currently: reminders).
class BotScheduler {
public:
    BotScheduler() = default;
    ~BotScheduler();

    void start();
    void stop();

private:
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace xipher

#endif // XIPHER_BOT_SCHEDULER_HPP


