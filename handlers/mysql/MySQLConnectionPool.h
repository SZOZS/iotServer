#pragma once
#ifndef MYSQL_CONNECTION_POOL_H
#define MYSQL_CONNECTION_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../../configs/ConfigMySQL.h"
#include "../../patterns/Singleton.h"

#include "MySQLHandler.h"

// MySQL连接池：基于C++14的线程安全连接池实现
class MySQLConnectionPool : public Singleton<MySQLConnectionPool>
{
public:
    // 禁用拷贝构造和赋值
    MySQLConnectionPool(const MySQLConnectionPool&) = delete;
    MySQLConnectionPool& operator=(const MySQLConnectionPool&) = delete;

    // 支持移动构造和移动赋值
    MySQLConnectionPool(MySQLConnectionPool&&) noexcept = delete;
    MySQLConnectionPool& operator=(MySQLConnectionPool&&) noexcept = delete;

    // 获取连接（超时等待机制）
    std::shared_ptr<MySQLHandler> getConnection(int timeout_ms = 1000);

    // 释放连接（放回池内）
    void releaseConnection(std::shared_ptr<MySQLHandler> conn);
    void emergencyCleanup();
    void monitorThreadFunc();

    // 获取当前连接池状态信息
    struct PoolStats
    {
        size_t idle_connections;
        size_t active_connections;
        size_t total_connections;
        size_t max_connections;
    };
    PoolStats getPoolStats() const;
    class ConnectionGuard
    {
    public:
        ConnectionGuard(std::shared_ptr<MySQLHandler> conn) : m_conn(std::move(conn)) {}
        ~ConnectionGuard()
        {
            if (m_conn) {
                MySQLConnectionPool::getInstance().releaseConnection(m_conn);
            }
        }
        // 禁止拷贝，允许移动
        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;
        ConnectionGuard(ConnectionGuard&&) noexcept = default;
        ConnectionGuard& operator=(ConnectionGuard&&) noexcept = default;

        // 提供连接访问
        std::shared_ptr<MySQLHandler> get() const { return m_conn; }
        std::shared_ptr<MySQLHandler> operator->() const { return m_conn; }

    private:
        std::shared_ptr<MySQLHandler> m_conn;
    };

private:
    friend Singleton<MySQLConnectionPool>;
    MySQLConnectionPool();
    ~MySQLConnectionPool();

    // 初始化连接池
    void initConnections();

    // 创建新连接或重置现有连接
    std::shared_ptr<MySQLHandler> createOrResetConnection();

    // 定期清理过期连接的线程函数
    void cleanupThreadFunc();

    ConfigMySQL& m_config;           // 配置引用
    mutable std::mutex m_mutex;      // 队列锁
    std::condition_variable m_cond;  // 等待条件变量
    size_t m_totalConnections;       // 总连接数
    size_t m_activeConnections;      // 活跃连接数

    struct ConnectionWrapper
    {
        std::shared_ptr<MySQLHandler> conn;
        std::chrono::steady_clock::time_point createTime;    // 连接创建时间
        std::chrono::steady_clock::time_point lastUsedTime;  // 最后使用时间
    };

    std::queue<ConnectionWrapper> m_idleConnections;  // 空闲连接队列
    std::unique_ptr<std::thread> m_cleanupThread;     // 清理线程
    std::unique_ptr<std::thread> m_monitorThread;     // 监控线程
    std::atomic<bool> m_running{true};                // 线程运行标志
};

#endif  // MYSQL_CONNECTION_POOL_H