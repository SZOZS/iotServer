#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

// 日志级别
enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

// 日志输出目标
enum class LogTarget
{
    CONSOLE,
    FILE,
    BOTH
};

// 单例日志类
class Logger
{
public:
    // 删除拷贝构造和赋值运算符
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 删除移动构造和移动赋值运算符
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static Logger& getInstance();                                                             // 获取单例实例
    void setLogLevel(LogLevel level);                                                         // 设置日志级别
    bool setLogFile(const std::string& filePath);                                             // 设置日志文件路径
    void setLogTarget(LogTarget target);                                                      // 设置日志输出目标
    void log(LogLevel level, const std::string& message, const std::string& file, int line);  // 日志输出函数

    // 基础日志函数（无延时）
    void debug(const std::string& message, const std::string& file, int line);
    void info(const std::string& message, const std::string& file, int line);
    void warning(const std::string& message, const std::string& file, int line);
    void error(const std::string& message, const std::string& file, int line);
    void fatal(const std::string& message, const std::string& file, int line);

    // 基础日志函数（1毫秒延时）
    void debugDelay(const std::string& message, const std::string& file, int line);
    void infoDelay(const std::string& message, const std::string& file, int line);
    void warningDelay(const std::string& message, const std::string& file, int line);
    void errorDelay(const std::string& message, const std::string& file, int line);
    void fatalDelay(const std::string& message, const std::string& file, int line);

private:
    Logger();   // 私有构造函数
    ~Logger();  // 私有析构函数

    std::string getCurrentTime();                                      // 格式化时间
    std::string levelToString(LogLevel level);                         // 日志级别转字符串
    void logToConsole(const std::string& logMessage, LogLevel level);  // 输出日志到控制台
    void logToFile(const std::string& logMessage);                     // 输出日志到文件

    std::mutex m_logger_mutex;       // 线程安全锁
    LogLevel m_logger_logLevel;      // 当前日志级别
    LogTarget m_logger_logTarget;    // 当前日志输出目标
    std::ofstream m_logger_logFile;  // 日志文件流
    std::string m_logger_filePath;   // 日志文件路径
};

// ========== 日志宏（无延时） ==========
#define LOG_DEBUG(message) Logger::getInstance().debug(message, __FILE__, __LINE__)
#define LOG_INFO(message) Logger::getInstance().info(message, __FILE__, __LINE__)
#define LOG_WARNING(message) Logger::getInstance().warning(message, __FILE__, __LINE__)
#define LOG_ERROR(message) Logger::getInstance().error(message, __FILE__, __LINE__)
#define LOG_FATAL(message) Logger::getInstance().fatal(message, __FILE__, __LINE__)

// ========== 日志宏（1毫秒延时） ==========
#define LOG_DEBUG_DELAY(message) Logger::getInstance().debugDelay(message, __FILE__, __LINE__)
#define LOG_INFO_DELAY(message) Logger::getInstance().infoDelay(message, __FILE__, __LINE__)
#define LOG_WARNING_DELAY(message) Logger::getInstance().warningDelay(message, __FILE__, __LINE__)
#define LOG_ERROR_DELAY(message) Logger::getInstance().errorDelay(message, __FILE__, __LINE__)
#define LOG_FATAL_DELAY(message) Logger::getInstance().fatalDelay(message, __FILE__, __LINE__)

// ========== 格式化日志宏（原有+新增延时版） ==========
// ========== 格式化日志宏（无延时） ==========
#define LOG_DEBUG_FMT(format, ...)                             \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_DEBUG(buffer);                                     \
    } while (0)
#define LOG_INFO_FMT(format, ...)                              \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_INFO(buffer);                                      \
    } while (0)
#define LOG_WARNING_FMT(format, ...)                           \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_WARNING(buffer);                                   \
    } while (0)
#define LOG_ERROR_FMT(format, ...)                             \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_ERROR(buffer);                                     \
    } while (0)
#define LOG_FATAL_FMT(format, ...)                             \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_FATAL(buffer);                                     \
    } while (0)
// ========== 格式化日志宏（1毫秒延时） ==========
#define LOG_DEBUG_FMT_DELAY(format, ...)                       \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_DEBUG_DELAY(buffer);                               \
    } while (0)
#define LOG_INFO_FMT_DELAY(format, ...)                        \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_INFO_DELAY(buffer);                                \
    } while (0)
#define LOG_WARNING_FMT_DELAY(format, ...)                     \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_WARNING_DELAY(buffer);                             \
    } while (0)
#define LOG_ERROR_FMT_DELAY(format, ...)                       \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_ERROR_DELAY(buffer);                               \
    } while (0)
#define LOG_FATAL_FMT_DELAY(format, ...)                       \
    do {                                                       \
        char buffer[1024];                                     \
        snprintf(buffer, sizeof(buffer), format, __VA_ARGS__); \
        LOG_FATAL_DELAY(buffer);                               \
    } while (0)
#endif  // LOGGER_H