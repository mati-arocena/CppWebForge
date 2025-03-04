#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <sstream>
#include <format>

namespace cppwebforge {

class Logger {
public:
    static Logger& getInstance();

    void setLogFile(const std::string& filename);
    
    void logInfo(std::string_view message) {
        logImpl(true, std::string(message));
    }
    
    template<typename... Args>
    void logInfo(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(true, std::format(fmt, std::forward<Args>(args)...));
    }

    #ifdef DEBUG_MODE
    void logDebug(std::string_view message) {
        logImpl(false, std::string(message));
    }
    
    template<typename... Args>
    void logDebug(std::format_string<Args...> fmt, Args&&... args) {
        logImpl(false, std::format(fmt, std::forward<Args>(args)...));
    }
    #endif

private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void logImpl(bool info, const std::string& message);

    class LoggerImpl;
    std::unique_ptr<LoggerImpl> impl_;
};

} // namespace cppwebforge

#define FORGE_INFO(...) cppwebforge::Logger::getInstance().logInfo(__VA_ARGS__)

#ifdef DEBUG_MODE
    #define FORGE_DEBUG(...) cppwebforge::Logger::getInstance().logDebug(__VA_ARGS__)
#else
    #define FORGE_DEBUG(...) ((void)0)
#endif 