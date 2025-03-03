#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <mutex>
#include <crow.h>
#include <vector>
#include <algorithm>

namespace cppwebforge {

static constexpr size_t MAX_LOG_LINES = 5000;

class Logger::LoggerImpl {
public:
    LoggerImpl() : consoleOutput(false), lineCount(0), maxLines(MAX_LOG_LINES) {
#ifdef DEBUG_MODE
        consoleOutput = true;
#endif
        setLogFile("application.log");
    }

    ~LoggerImpl() {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    void setLogFile(const std::string& filename) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        if (logFile.is_open()) {
            logFile.close();
        }
        
        logFilename = filename;
        
        std::ifstream fileCheck(filename);
        if (fileCheck.is_open()) {
            lineCount = 0;
            std::string line;
            while (std::getline(fileCheck, line)) {
                lineCount++;
            }
            fileCheck.close();
        } else {
            lineCount = 0;
        }
        
        logFile.open(filename, std::ios::app);
        
        if (!logFile.is_open()) {
            CROW_LOG_ERROR << "Failed to open log file: " << filename;
        }
    }

    void rotateLogIfNeeded() {
        if (lineCount >= maxLines) {
            if (logFile.is_open()) {
                logFile.close();
            }
            
            std::vector<std::string> logLines;
            std::ifstream inFile(logFilename);
            if (inFile.is_open()) {
                std::string line;
                while (std::getline(inFile, line)) {
                    logLines.push_back(line);
                }
                inFile.close();
            }
            
            if (logLines.size() > maxLines / 2) {
                auto keepStart = logLines.begin() + static_cast<std::ptrdiff_t>(logLines.size() - maxLines / 2);
                logLines.erase(logLines.begin(), keepStart);
            }
            
            std::ofstream outFile(logFilename, std::ios::trunc);
            if (outFile.is_open()) {
                for (const auto& line : logLines) {
                    outFile << line << "\n";
                }
                outFile.close();
            }
            
            lineCount = logLines.size();
            logFile.open(logFilename, std::ios::app);
        }
    }

    void logWithLevel(const std::string& level, const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex);
        
        const int MILLIS_IN_SECOND = 1000;
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % MILLIS_IN_SECOND;
        
        std::stringstream strStream;
        strStream << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << millis.count()
           << " [" << level << "] " << message;
        
        std::string formattedMessage = strStream.str();
        
        if (logFile.is_open()) {
            rotateLogIfNeeded();
            
            logFile << formattedMessage << "\n";
            logFile.flush();
            lineCount++;
        }
    }

    void logImpl(bool info, const std::string& message) {
        logWithLevel(info ? "INFO" : "DEBUG", message);
        
        if (consoleOutput) {
            CROW_LOG_INFO << message;
        }
    }

    std::ofstream logFile;
    std::mutex logMutex;
    bool consoleOutput;
    std::string logFilename;
    size_t lineCount;
    const size_t maxLines;
};

Logger::Logger() : impl_(std::make_unique<LoggerImpl>()) {}

Logger::~Logger() = default;

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLogFile(const std::string& filename) {
    impl_->setLogFile(filename);
}

void Logger::logImpl(bool info, const std::string& message) {
    impl_->logImpl(info, message);
}

} // namespace cppwebforge
