#pragma once

#include <string>
#include <memory>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iostream>

namespace crossexpand {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

class Logger {
public:
    static Logger& Instance();
    
    void SetLevel(LogLevel level);
    void SetOutputFile(const std::string& filename);
    
    template<typename... Args>
    void Log(LogLevel level, const std::string& format, Args&&... args) {
        if (level < min_level_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto message = FormatMessage(format, std::forward<Args>(args)...);
        auto timestamp = GetTimestamp();
        auto level_str = GetLevelString(level);
        
        std::string log_line = timestamp + " [" + level_str + "] " + message + "\n";
        
        // Console output
        std::cout << log_line;
        
        // File output
        if (file_stream_.is_open()) {
            file_stream_ << log_line;
            file_stream_.flush();
        }
    }

private:
    Logger() : min_level_(LogLevel::INFO) {}
    ~Logger() = default;
    
    LogLevel min_level_;
    std::ofstream file_stream_;
    mutable std::mutex mutex_;
    
    template<typename... Args>
    std::string FormatMessage(const std::string& format, Args&&... args) {
        std::ostringstream oss;
        FormatImpl(oss, format, std::forward<Args>(args)...);
        return oss.str();
    }
    
    void FormatImpl(std::ostringstream& oss, const std::string& format) {
        oss << format;
    }
    
    template<typename T, typename... Args>
    void FormatImpl(std::ostringstream& oss, const std::string& format, T&& value, Args&&... args) {
        size_t pos = format.find("{}");
        if (pos != std::string::npos) {
            oss << format.substr(0, pos) << value;
            FormatImpl(oss, format.substr(pos + 2), std::forward<Args>(args)...);
        } else {
            oss << format;
        }
    }
    
    std::string GetTimestamp();
    std::string GetLevelString(LogLevel level);
};

// Convenience macros
#define LOG_DEBUG(...) Logger::Instance().Log(LogLevel::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) Logger::Instance().Log(LogLevel::INFO, __VA_ARGS__)
#define LOG_WARNING(...) Logger::Instance().Log(LogLevel::WARNING, __VA_ARGS__)
#define LOG_ERROR(...) Logger::Instance().Log(LogLevel::ERROR, __VA_ARGS__)
#define LOG_FATAL(...) Logger::Instance().Log(LogLevel::FATAL, __VA_ARGS__)

} // namespace crossexpand
