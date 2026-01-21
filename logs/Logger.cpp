#include <cstdio>
#include <stdexcept>

#include "Logger.h"

Logger::Logger()
    : m_logger_logLevel(LogLevel::DEBUG)
    ,                                       // 默认日志级别为DEBUG
    m_logger_logTarget(LogTarget::CONSOLE)  // 默认只输出到控制台
{
    // 初始化时不打开日志文件，等待用户设置
}

Logger::~Logger()
{
    if (m_logger_logFile.is_open())
        m_logger_logFile.close();
}

void Logger::setLogLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(m_logger_mutex);
    m_logger_logLevel = level;
}

bool Logger::setLogFile(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(m_logger_mutex);

    // 关闭当前文件（如果打开）
    if (m_logger_logFile.is_open())
        m_logger_logFile.close();

    // 尝试打开新文件
    m_logger_logFile.open(filePath, std::ios::app | std::ios::out);
    if (m_logger_logFile.is_open()) {
        m_logger_filePath = filePath;
        return true;
    }

    return false;
}

void Logger::setLogTarget(LogTarget target)
{
    std::lock_guard<std::mutex> lock(m_logger_mutex);
    m_logger_logTarget = target;
}

std::string Logger::getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_r(&time, &localTime);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - std::chrono::system_clock::from_time_t(time)).count();
    std::stringstream ss;
    ss << std::put_time(&localTime, "%y%m%d%H%M%S") << "." << std::setw(3) << std::setfill('0') << ms;
    return ss.str();
}

std::string Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARN";
        case LogLevel::ERROR:
            return "ERRO";
        case LogLevel::FATAL:
            return "FATA";
        default:
            return "UNKN";
    }
}

// 核心修改：根据传入的 LogLevel level 显示对应颜色
void Logger::logToConsole(const std::string& logMessage, LogLevel level)
{
    // 不同级别对应不同 ANSI 颜色码
    switch (level) {
        case LogLevel::DEBUG:
            std::cout << "\033[37m";
            break;  // 灰色（DEBUG）
        case LogLevel::INFO:
            std::cout << "\033[32m";
            break;  // 绿色（INFO）
        case LogLevel::WARNING:
            std::cout << "\033[33m";
            break;  // 黄色（WARNING）
        case LogLevel::ERROR:
            std::cout << "\033[31m";
            break;  // 红色（ERROR）
        case LogLevel::FATAL:
            std::cout << "\033[1;31m";
            break;  // 亮红色（FATAL，加粗）
        default:
            std::cout << "\033[0m";
            break;  // 默认颜色
    }
    // 输出日志并重置颜色
    std::cout << logMessage << "\033[0m" << std::endl;
}

void Logger::logToFile(const std::string& logMessage)
{
    if (m_logger_logFile.is_open()) {
        m_logger_logFile << logMessage << std::endl;
    }
}

// 第三步：修改 log 函数中调用 logToConsole 的地方，传入当前日志级别
void Logger::log(LogLevel level, const std::string& message, const std::string& file, int line)
{
    if (level < m_logger_logLevel)
        return;

    std::lock_guard<std::mutex> lock(m_logger_mutex);

    // 组合 file 和 line 为 "file:line" 格式
    size_t lastSlash = file.find_last_of("/");
    std::string fileName = (lastSlash == std::string::npos) ? file : file.substr(lastSlash + 1);
    // 组合为 "文件名:行号" 格式
    std::string fileLine = fileName + ":" + std::to_string(line);

    // 构建日志消息
    std::stringstream ss;
    ss << getCurrentTime() << "-" << levelToString(level) << "| " << message << " <=" << fileLine;

    std::string logMessage = ss.str();

    // 控制台输出（添加颜色控制）
    if (m_logger_logTarget == LogTarget::CONSOLE || m_logger_logTarget == LogTarget::BOTH) {
        logToConsole(logMessage, level);  // 新增 level 参数
    }

    // 文件输出（纯文本，无颜色）
    if (m_logger_logTarget == LogTarget::FILE || m_logger_logTarget == LogTarget::BOTH) {
        logToFile(logMessage);
    }
}

void Logger::debug(const std::string& message, const std::string& file, int line)
{
    log(LogLevel::DEBUG, message, file, line);
}

void Logger::info(const std::string& message, const std::string& file, int line)
{
    log(LogLevel::INFO, message, file, line);
}

void Logger::warning(const std::string& message, const std::string& file, int line)
{
    log(LogLevel::WARNING, message, file, line);
}

void Logger::error(const std::string& message, const std::string& file, int line)
{
    log(LogLevel::ERROR, message, file, line);
}

void Logger::fatal(const std::string& message, const std::string& file, int line)
{
    log(LogLevel::FATAL, message, file, line);
}
