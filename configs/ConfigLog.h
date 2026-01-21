#pragma once
#ifndef CONFIG_LOG_H
#define CONFIG_LOG_H

#include "ConfigGlobal.h"

// 日志配置类（单例模式）
class ConfigLog
{
private:
    // 私有构造函数：从全局配置初始化
    ConfigLog()
    {
        auto& globalLog = ConfigGlobal::getInstance().getConfigLog();
        m_filePath = globalLog.filePath;
    }

    // 禁止拷贝和赋值
    ConfigLog(const ConfigLog&) = delete;
    ConfigLog& operator=(const ConfigLog&) = delete;

    std::string m_filePath;  // 日志文件路径

public:
    // 获取单例实例
    static ConfigLog& getInstance()
    {
        static ConfigLog instance;
        return instance;
    }

    // 获取日志文件路径
    std::string getLogFilePath() const { return m_filePath; }

    // 动态修改日志文件路径（可选）
    void setLogFilePath(const std::string& path) { m_filePath = path; }
};

#endif  // CONFIG_LOG_H