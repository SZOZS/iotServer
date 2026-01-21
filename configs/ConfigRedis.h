#pragma once
#ifndef CONFIG_REDIS_H
#define CONFIG_REDIS_H

#include <string>
#include <vector>

#include "ConfigGlobal.h"

class ConfigRedis
{
private:
    ConfigRedis()
    {
        auto& globalRedis = ConfigGlobal::getInstance().getConfigRedis();
        m_host = globalRedis.host;                                    // 主机地址
        m_port = globalRedis.port;                                    // 端口
        m_password = globalRedis.password;                            // 密码
        m_db = globalRedis.db;                                        // 数据库编号
        m_timeout = globalRedis.timeout;                              // 连接超时（毫秒）
        m_maxRetries = globalRedis.maxRetries;                        // 最大重试次数
        m_retryDelay = globalRedis.retryDelay;                        // 重连延迟（毫秒）
        m_maxConnections = globalRedis.maxConnections;                // 连接池最大连接数
        m_connectionMaxLifetime = globalRedis.connectionMaxLifetime;  // 连接最大存活时间（秒）
        m_maxIdleTime = globalRedis.maxIdleTime;                      // 最大空闲时间（秒）
    }

    ~ConfigRedis() = default;

    std::string m_host;
    unsigned int m_port;
    std::string m_password;
    int m_db;
    unsigned int m_timeout;
    unsigned int m_maxRetries;
    unsigned int m_retryDelay;
    unsigned int m_maxConnections;
    unsigned int m_connectionMaxLifetime;
    unsigned int m_maxIdleTime;

public:
    // 禁止拷贝构造和赋值
    ConfigRedis(const ConfigRedis&) = delete;
    ConfigRedis& operator=(const ConfigRedis&) = delete;

    // 获取单例实例
    static ConfigRedis& getInstance()
    {
        static ConfigRedis instance;
        return instance;
    }

    // 获取配置项
    std::string getHost() const { return m_host; }
    unsigned int getPort() const { return m_port; }
    std::string getPassword() const { return m_password; }
    int getDb() const { return m_db; }
    unsigned int getTimeout() const { return m_timeout; }
    unsigned int getMaxRetries() const { return m_maxRetries; }
    unsigned int getRetryDelay() const { return m_retryDelay; }
    unsigned int getMaxConnections() const { return m_maxConnections; }
    unsigned int getConnectionMaxLifetime() const { return m_connectionMaxLifetime; }
    unsigned int getMaxIdleTime() const { return m_maxIdleTime; }
};

#endif  // CONFIG_REDIS_H