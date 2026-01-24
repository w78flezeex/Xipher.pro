#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

namespace xipher {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();
    
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    void setLogFile(const std::string& filename);
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::mutex mutex_;
    std::ofstream log_file_;
    bool file_logging_enabled_ = false;
    
    std::string levelToString(LogLevel level);
    std::string getCurrentTime();
};

} // namespace xipher

#endif // LOGGER_HPP

