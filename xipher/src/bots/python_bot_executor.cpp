#include "../include/bots/python_bot_executor.hpp"
#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include "../include/utils/json_parser.hpp"

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <mutex>

namespace fs = boost::filesystem;

namespace xipher {

namespace {

static std::mutex g_cache_mutex;
static std::unordered_map<std::string, size_t> g_code_hash;
static const std::string kRunnerPath = "/root/xipher/scripts/python_bot_runner.py";
static const std::string kCacheDir = "/tmp/xipher_bot_scripts";

static size_t stableHash(const std::string& s) {
    // FNV-1a 64-bit
    size_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= static_cast<size_t>(c);
        h *= 1099511628211ull;
    }
    return h;
}

static std::string ensureScriptFile(const std::string& bot_user_id, const std::string& code) {
    try {
        fs::create_directories(kCacheDir);
    } catch (...) {
        // ignore
    }
    const size_t h = stableHash(code);
    const std::string path = kCacheDir + "/" + bot_user_id + ".py";
    
    {
        std::lock_guard<std::mutex> lock(g_cache_mutex);
        // Always write file to ensure latest code is used
        // Cache check is just for optimization, but we always update the file
        g_code_hash[bot_user_id] = h;
    }

    // Always write the file to ensure latest code is used
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return "";
    out << code;
    out.flush();
    out.close();
    
    Logger::getInstance().info("Python bot script file updated for bot_user_id: " + bot_user_id + " (hash: " + std::to_string(h) + ")");
    return path;
}

static bool setLimits() {
    // best-effort sandboxing: CPU/mem/files
    struct rlimit rl;
    // Increase CPU limit to 30 seconds (was 1 second, causing exit code 120)
    rl.rlim_cur = 30;
    rl.rlim_max = 30;
    setrlimit(RLIMIT_CPU, &rl);

    rl.rlim_cur = 256 * 1024 * 1024; // 256MB
    rl.rlim_max = 256 * 1024 * 1024;
    setrlimit(RLIMIT_AS, &rl);

    rl.rlim_cur = 64;
    rl.rlim_max = 64;
    setrlimit(RLIMIT_NOFILE, &rl);
    return true;
}

static bool writeAll(int fd, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        ssize_t n = ::write(fd, data.data() + off, data.size() - off);
        if (n <= 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

static std::string readWithTimeout(int fd, int timeout_ms) {
    std::string out;
    char buf[4096];
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    int remaining = timeout_ms;
    const int step = 100; // Check every 100ms

    while (remaining > 0) {
        int r = ::poll(&pfd, 1, step);
        if (r > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) {
                out.append(buf, buf + n);
                if (out.size() > 64 * 1024) break; // cap output
                continue;
            } else if (n == 0) {
                // EOF - pipe closed
                break;
            } else {
                // Error reading
                break;
            }
        } else if (r == 0) {
            // Timeout, but continue waiting
            remaining -= step;
        } else {
            // Poll error
            break;
        }
        remaining -= step;
    }
    return out;
}

static std::string buildUpdateJson(const PythonBotEvent& ev) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"" << JsonParser::escapeJson(ev.type) << "\","
        << "\"scope\":\"" << JsonParser::escapeJson(ev.scope) << "\","
        << "\"scope_id\":\"" << JsonParser::escapeJson(ev.scope_id) << "\","
        << "\"from_user_id\":\"" << JsonParser::escapeJson(ev.from_user_id) << "\","
        << "\"from_username\":\"" << JsonParser::escapeJson(ev.from_username) << "\","
        << "\"username\":\"" << JsonParser::escapeJson(ev.from_username) << "\","
        << "\"from_role\":\"" << JsonParser::escapeJson(ev.from_role) << "\","
        << "\"text\":\"" << JsonParser::escapeJson(ev.text) << "\","
        << "\"joined_username\":\"" << JsonParser::escapeJson(ev.joined_username) << "\""
        << "}";
    return oss.str();
}

static void executeActions(DatabaseManager& db,
                           const std::string& bot_user_id,
                           const PythonBotEvent& ev,
                           const boost::property_tree::ptree& root) {
    // actions: list of {type, text, ...}
    auto actionsOpt = root.get_child_optional("actions");
    if (!actionsOpt) return;

    int count = 0;
    for (const auto& child : *actionsOpt) {
        if (count++ >= 5) break;
        const auto& a = child.second;
        const std::string type = a.get<std::string>("type", "");
        const std::string text = a.get<std::string>("text", "");
        if (type.empty()) continue;

        const std::string clipped = (text.size() > 2000) ? text.substr(0, 2000) : text;

        if (type == "send") {
            if (clipped.empty()) continue;
            
            // Parse reply_markup if present
            std::string reply_markup_json = "";
            auto replyMarkupOpt = a.get_child_optional("reply_markup");
            if (replyMarkupOpt) {
                std::stringstream ss;
                boost::property_tree::write_json(ss, *replyMarkupOpt, false);
                reply_markup_json = ss.str();
            }
            
            if (ev.scope == "direct") {
                db.sendMessage(bot_user_id, ev.from_user_id, clipped, "text", "", "", 0, "", "", "", "", reply_markup_json);
            } else if (ev.scope == "group") {
                db.sendGroupMessage(ev.scope_id, bot_user_id, clipped, "text", "", "", 0, "", reply_markup_json);
            }
        } else if (type == "send_dm") {
            const std::string uid = a.get<std::string>("user_id", "");
            if (uid.empty() || clipped.empty()) continue;
            
            std::string reply_markup_json = "";
            auto replyMarkupOpt = a.get_child_optional("reply_markup");
            if (replyMarkupOpt) {
                std::stringstream ss;
                boost::property_tree::write_json(ss, *replyMarkupOpt, false);
                reply_markup_json = ss.str();
            }
            
            db.sendMessage(bot_user_id, uid, clipped, "text", "", "", 0, "", "", "", "", reply_markup_json);
        } else if (type == "send_group") {
            const std::string gid = a.get<std::string>("group_id", "");
            if (gid.empty() || clipped.empty()) continue;
            
            std::string reply_markup_json = "";
            auto replyMarkupOpt = a.get_child_optional("reply_markup");
            if (replyMarkupOpt) {
                std::stringstream ss;
                boost::property_tree::write_json(ss, *replyMarkupOpt, false);
                reply_markup_json = ss.str();
            }
            
            db.sendGroupMessage(gid, bot_user_id, clipped, reply_markup_json);
        }
    }
}

} // namespace

bool PythonBotExecutor::handle(DatabaseManager& db,
                              const std::string& bot_user_id,
                              const PythonBotEvent& ev) {
    std::string lang;
    bool enabled = false;
    std::string code;
    if (!db.getBotBuilderScriptByBotUserId(bot_user_id, lang, enabled, code)) {
        Logger::getInstance().warning("Python bot: failed to get script for bot_user_id: " + bot_user_id);
        return false;
    }
    std::transform(lang.begin(), lang.end(), lang.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if (!enabled || lang != "python" || code.empty()) {
        Logger::getInstance().info("Python bot: script disabled or empty for bot_user_id: " + bot_user_id + " (enabled=" + (enabled ? "true" : "false") + ", lang=" + lang + ", code_len=" + std::to_string(code.length()) + ")");
        return false;
    }
    
    Logger::getInstance().info("Python bot: executing script for bot_user_id: " + bot_user_id + " (code length: " + std::to_string(code.length()) + ")");

    // Get bot_id from bot_user_id to load all project files
    auto bot = db.getBotBuilderBotByUserId(bot_user_id);
    if (bot.id.empty()) {
        Logger::getInstance().warning("Python bot: failed to get bot info for bot_user_id: " + bot_user_id);
        return false;
    }
    
    // Load all bot files to create project structure
    std::vector<DatabaseManager::BotFile> bot_files = db.listBotFiles(bot.id);
    bool hasProjectFiles = false;
    const std::string projectDir = kCacheDir + "/" + bot_user_id + "_project";
    
    if (!bot_files.empty()) {
        hasProjectFiles = true;
        // Create project directory
        try {
            fs::create_directories(projectDir);
            
            // Write all files to project directory
            for (const auto& file : bot_files) {
                const std::string filePath = projectDir + "/" + file.file_path;
                const std::string fileDir = fs::path(filePath).parent_path().string();
                if (!fileDir.empty() && fileDir != projectDir) {
                    fs::create_directories(fileDir);
                }
                std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
                if (out) {
                    out << file.file_content;
                    out.close();
                }
            }
            Logger::getInstance().info("Python bot: created project structure with " + std::to_string(bot_files.size()) + " files");
            
            // Install dependencies from requirements.txt if exists (optional, non-blocking)
            const std::string requirementsPath = projectDir + "/requirements.txt";
            if (fs::exists(requirementsPath)) {
                Logger::getInstance().info("Python bot: 📦 Found requirements.txt, attempting to install dependencies (non-blocking)...");
                // Try to install dependencies in background, don't block bot execution
                pid_t pip_pid = ::fork();
                if (pip_pid == 0) {
                    // child process for pip install
                    ::chdir(projectDir.c_str());
                    // Redirect output to /dev/null to avoid blocking
                    int devnull = ::open("/dev/null", O_WRONLY);
                    ::dup2(devnull, STDOUT_FILENO);
                    ::dup2(devnull, STDERR_FILENO);
                    ::close(devnull);
                    // Try pip3 first, then python3 -m pip (if available)
                    ::execlp("pip3", "pip3", "install", "--user", "-q", "-r", "requirements.txt", (char*)nullptr);
                    ::execlp("python3", "python3", "-m", "pip", "install", "--user", "-q", "-r", "requirements.txt", (char*)nullptr);
                    _exit(0); // Don't treat as error if pip not available
                } else if (pip_pid > 0) {
                    // parent - don't wait, let it install in background
                    // Just wait a bit for quick installs (max 2 seconds)
                    ::usleep(2000000); // 2 seconds
                    pid_t w = ::waitpid(pip_pid, nullptr, WNOHANG);
                    if (w == pip_pid) {
                        Logger::getInstance().info("Python bot: ✅ Dependencies installation completed");
                    } else {
                        Logger::getInstance().info("Python bot: ℹ️ Dependencies installation in progress (non-blocking, bot will work without them)");
                    }
                }
            }
        } catch (const std::exception& e) {
            Logger::getInstance().warning("Python bot: failed to create project structure: " + std::string(e.what()));
        }
    }
    
    // Use main.py from project directory, or fallback to script_code
    const std::string mainPyPath = projectDir + "/main.py";
    std::string botPath;
    if (hasProjectFiles && fs::exists(mainPyPath)) {
        botPath = mainPyPath;
        Logger::getInstance().info("Python bot: 🚀 Starting bot with main.py from project directory: " + botPath);
    } else {
        // Fallback to single file execution
        botPath = ensureScriptFile(bot_user_id, code);
        if (botPath.empty()) {
            Logger::getInstance().warning("Python bot: failed to write script file");
            return true;
        }
    }

    int in_pipe[2];
    int out_pipe[2];
    if (::pipe(in_pipe) != 0) return true;
    if (::pipe(out_pipe) != 0) {
        ::close(in_pipe[0]); ::close(in_pipe[1]);
        return true;
    }

    // Create separate pipe for stderr to capture Python errors
    int err_pipe[2];
    if (::pipe(err_pipe) != 0) {
        ::close(in_pipe[0]); ::close(in_pipe[1]);
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        return true;
    }

    pid_t pid = ::fork();
    if (pid == 0) {
        // child
        ::dup2(in_pipe[0], STDIN_FILENO);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(err_pipe[1], STDERR_FILENO); // Capture stderr separately
        ::close(in_pipe[0]); ::close(in_pipe[1]);
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        ::close(err_pipe[0]); ::close(err_pipe[1]);

        setLimits();

        // Set PYTHONPATH to project directory if using project structure
        if (hasProjectFiles && fs::exists(projectDir)) {
            std::string pythonpath = projectDir;
            const char* existing_pythonpath = ::getenv("PYTHONPATH");
            if (existing_pythonpath && strlen(existing_pythonpath) > 0) {
                pythonpath = std::string(existing_pythonpath) + ":" + projectDir;
            }
            ::setenv("PYTHONPATH", pythonpath.c_str(), 1);
            // Also change working directory to project directory
            ::chdir(projectDir.c_str());
            
            // Add user site-packages to PYTHONPATH for --user installed packages
            const char* home = ::getenv("HOME");
            if (home) {
                // Try common Python version site-packages paths
                std::vector<std::string> user_paths = {
                    std::string(home) + "/.local/lib/python3.10/site-packages",
                    std::string(home) + "/.local/lib/python3.11/site-packages",
                    std::string(home) + "/.local/lib/python3.12/site-packages",
                    std::string(home) + "/.local/lib/python3.13/site-packages"
                };
                std::string pythonpath_with_user = pythonpath;
                for (const auto& user_path : user_paths) {
                    if (fs::exists(user_path) && pythonpath_with_user.find(user_path) == std::string::npos) {
                        pythonpath_with_user = user_path + ":" + pythonpath_with_user;
                    }
                }
                if (pythonpath_with_user != pythonpath) {
                    ::setenv("PYTHONPATH", pythonpath_with_user.c_str(), 1);
                }
            }
        }
        
        const char* argv[] = {
            "python3",
            "-I",
            kRunnerPath.c_str(),
            botPath.c_str(),
            nullptr
        };
        ::execvp("python3", (char* const*)argv);
        _exit(127);
    }

    // parent
    ::close(in_pipe[0]);
    ::close(out_pipe[1]);
    ::close(err_pipe[1]); // Close write end of stderr pipe

    const std::string updateJson = buildUpdateJson(ev);
    writeAll(in_pipe[1], updateJson);
    ::close(in_pipe[1]);

    // read output and stderr with timeout (30 seconds to allow for HTTP requests, etc.)
    Logger::getInstance().info("Python bot: Reading bot output (timeout: 30s)...");
    std::string out = readWithTimeout(out_pipe[0], 30000);
    std::string err = readWithTimeout(err_pipe[0], 1000); // Read stderr (1 second timeout)
    ::close(out_pipe[0]);
    ::close(err_pipe[0]);
    
    Logger::getInstance().info("Python bot: Finished reading output, got " + std::to_string(out.length()) + " bytes stdout, " + std::to_string(err.length()) + " bytes stderr");
    if (!err.empty()) {
        Logger::getInstance().warning("Python bot: ⚠️ stderr output: " + err.substr(0, 2000));
        // Append stderr to output if stdout is empty
        if (out.empty() && !err.empty()) {
            out = err;
            Logger::getInstance().info("Python bot: Using stderr as output since stdout is empty");
        }
    }
    
    if (out.empty() && err.empty()) {
        Logger::getInstance().warning("Python bot: ⚠️ No output received from bot (neither stdout nor stderr)");
    }
    
    // Wait for bot completion (up to 30 seconds) without double-reaping.
    int status = 0;
    int waited = 0;
    bool finished = false;
    while (waited < 30000) {
        pid_t w = ::waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            if (WIFEXITED(status)) {
                Logger::getInstance().info("Python bot: Process exited with code " + std::to_string(WEXITSTATUS(status)));
            } else if (WIFSIGNALED(status)) {
                Logger::getInstance().warning("Python bot: Process killed by signal " + std::to_string(WTERMSIG(status)));
            }
            Logger::getInstance().info("Python bot: Bot process completed in " + std::to_string(waited) + "ms");
            finished = true;
            break;
        }
        if (w < 0) {
            Logger::getInstance().warning("Python bot: waitpid failed, assuming process already reaped");
            finished = true;
            break;
        }
        ::usleep(50 * 1000);
        waited += 50;
    }
    if (!finished) {
        ::kill(pid, SIGKILL);
        ::waitpid(pid, &status, 0);
        Logger::getInstance().warning("Python bot: ⏱️ Bot execution timed out after 30 seconds");
        return true;
    }

    // Log raw output for debugging (always log, even if empty)
    if (out.length() > 500) {
        Logger::getInstance().info("Python bot: 📤 Bot output (first 500 chars): " + out.substr(0, 500) + "...");
    } else if (!out.empty()) {
        Logger::getInstance().info("Python bot: 📤 Bot output: " + out);
    } else {
        Logger::getInstance().warning("Python bot: ⚠️ Bot produced no output");
        return true;
    }

    // Parse JSON output from runner
    // Try to find JSON in the output (bot might print errors before JSON)
    std::string json_output = out;
    size_t json_start = out.find("{");
    if (json_start != std::string::npos) {
        // Extract JSON part (from first { to last })
        size_t json_end = out.rfind("}");
        if (json_end != std::string::npos && json_end > json_start) {
            json_output = out.substr(json_start, json_end - json_start + 1);
            if (json_start > 0) {
                std::string error_part = out.substr(0, json_start);
                Logger::getInstance().warning("Python bot: ⚠️ Bot printed errors before JSON: " + error_part.substr(0, 300));
            }
        }
    }
    
    try {
        std::stringstream ss(json_output);
        boost::property_tree::ptree root;
        boost::property_tree::read_json(ss, root);
        const bool ok = root.get<bool>("ok", false);
        if (!ok) {
            const std::string err = root.get<std::string>("error", "unknown error");
            Logger::getInstance().warning("Python bot: ❌ Error: " + err);
            return true;
        }
        Logger::getInstance().info("Python bot: ✅ Bot executed successfully");
        executeActions(db, bot_user_id, ev, root);
    } catch (const std::exception& e) {
        Logger::getInstance().warning("Python bot: ❌ Output parse failed: " + std::string(e.what()));
        Logger::getInstance().warning("Python bot: Raw output was: " + out);
    }

    return true;
}

} // namespace xipher
