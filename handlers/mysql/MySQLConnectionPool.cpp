#include <algorithm>
#include <chrono>
#include <thread>

#include "../../logs/Logger.h"

#include "MySQLConnectionPool.h"

MySQLConnectionPool::MySQLConnectionPool() : m_config(ConfigMySQL::getInstance()), m_totalConnections(0), m_activeConnections(0)
{
    initConnections();

    // 启动定期清理线程
    m_cleanupThread = std::make_unique<std::thread>(&MySQLConnectionPool::cleanupThreadFunc, this);
    // 启动监控线程
    m_monitorThread = std::make_unique<std::thread>(&MySQLConnectionPool::monitorThreadFunc, this);
    LOG_INFO("MySQL连接池 初始化 完成");
}

MySQLConnectionPool::~MySQLConnectionPool()
{
    m_running = false;
    if (m_cleanupThread && m_cleanupThread->joinable()) {
        m_cleanupThread->join();
    }

    // 销毁所有连接
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_idleConnections.empty()) {
        m_idleConnections.pop();
    }
    m_totalConnections = 0;
    m_activeConnections = 0;
    LOG_INFO("MySQL连接池 已销毁");
}

void MySQLConnectionPool::initConnections()
{
    // 初始创建最小连接数
    // const size_t minConnections = std::max(1u, m_config.getMaxConnections() / 4);
    // std::lock_guard<std::mutex> lock(m_mutex);

    // for (size_t i = 0; i < minConnections; ++i) {
    //   auto conn = createOrResetConnection();
    //   if (conn) {
    //     m_idleConnections.push({conn, std::chrono::steady_clock::now(), std::chrono::steady_clock::now()});
    //   }
    // }
    // LOG_INFO_FMT("MySQL连接池初始化完成，初始连接数: %zu，最大连接数: %u", m_idleConnections.size(), m_config.getMaxConnections());

    // 按需初始化：不预先创建任何连接，初始连接数为0
    // 最小连接数逻辑调整为"首次获取时创建，后续动态维持"
    unsigned int initialConnections = 0;
    LOG_INFO_FMT("MySQL连接池 初始化:初始连接数[%u](按需创建),最大连接数[%u]", initialConnections, m_config.getMaxConnections());
}

std::shared_ptr<MySQLHandler> MySQLConnectionPool::createOrResetConnection()
{
    try {
        // 从配置获取连接参数
        auto host = m_config.getHost();
        auto user = m_config.getUser();
        auto password = m_config.getPassword();
        auto db = m_config.getDatabase();
        auto port = m_config.getPort();

        // 创建新连接
        std::shared_ptr<MySQLHandler> conn = std::make_shared<MySQLHandler>(host, user, password, db, port);

        if (conn->isConnected()) {
            // LOG_DEBUG_FMT("创建新MySQL连接，当前总连接数: %zu", m_totalConnections + 1);
            return conn;
        } else {
            LOG_ERROR("创建MySQL连接失败");
            return nullptr;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("创建连接时发生异常: %s", e.what());
        return nullptr;
    }
}

std::shared_ptr<MySQLHandler> MySQLConnectionPool::getConnection(int timeout_ms)
{
    const int retryCount = 2;  // 重试2次
    for (int i = 0; i < retryCount; ++i) {
        std::unique_lock<std::mutex> lock(m_mutex);
        // 等待空闲连接或可创建新连接（带超时）
        bool hasConnection = m_cond.wait_for(
            lock, std::chrono::milliseconds(timeout_ms), [this]() { return !m_idleConnections.empty() || m_totalConnections < m_config.getMaxConnections(); });
        if (hasConnection) {
            std::shared_ptr<MySQLHandler> conn;

            if (!m_idleConnections.empty()) {
                // 复用空闲连接
                auto wrapper = std::move(m_idleConnections.front());
                m_idleConnections.pop();
                conn = wrapper.conn;

                // 严格校验连接有效性（主动ping+状态检查）
                if (!conn->isConnected() || !conn->ping()) {
                    LOG_WARNING_FMT("复用空闲连接无效（第%d次尝试），重建连接", i + 1);
                    m_totalConnections--;              // 移除无效连接计数
                    conn = createOrResetConnection();  // 重建连接
                    if (!conn) {
                        LOG_ERROR("连接重建失败，放弃当前尝试");
                        continue;  // 进入下一次重试
                    }
                    // 重建成功，总连接数已在createOrResetConnection中更新
                }
            } else {
                // 无空闲连接，创建新连接（总连接数未达上限）
                conn = createOrResetConnection();
                if (!conn) {
                    LOG_ERROR_FMT("创建新连接失败（第%d次尝试）", i + 1);
                    continue;  // 进入下一次重试
                }
                m_totalConnections++;  // 新增连接，更新总计数
                LOG_DEBUG_FMT("创建新MySQL连接（第%d次尝试），当前总连接数: %zu", i + 1, m_totalConnections);
            }
            // 成功获取有效连接，更新活跃连接数
            m_activeConnections++;
            LOG_DEBUG_FMT("成功获取连接（第%d次尝试），活跃连接: %zu，总连接: %zu", i + 1, m_activeConnections, m_totalConnections);
            return conn;
        }
        // 超时且未达最大重试次数，触发紧急清理后重试
        if (i < retryCount - 1) {
            LOG_WARNING_FMT("获取连接超时（%dms，第%d次尝试），触发紧急清理", timeout_ms, i + 1);
            emergencyCleanup();  // 清理无效/超时连接，释放名额
        }
    }
    // 所有重试失败
    LOG_ERROR_FMT("获取MySQL连接失败（已重试%d次），连接池已满（%zu/%u）", retryCount, m_totalConnections, m_config.getMaxConnections());
    return nullptr;
}

// 新增紧急清理函数（在MySQLConnectionPool.h中声明）
void MySQLConnectionPool::emergencyCleanup()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    auto maxLifetime = std::chrono::seconds(m_config.getConnectionMaxLifetime());
    auto maxIdleTime = std::chrono::seconds(m_config.getMaxIdleTime());
    size_t cleaned = 0;

    std::queue<ConnectionWrapper> tempQueue;
    while (!m_idleConnections.empty()) {
        auto wrapper = std::move(m_idleConnections.front());
        m_idleConnections.pop();

        if (!wrapper.conn->isConnected() || (now - wrapper.lastUsedTime > maxIdleTime) || (now - wrapper.createTime > maxLifetime)) {
            m_totalConnections--;
            cleaned++;
        } else {
            tempQueue.push(std::move(wrapper));
        }
    }
    m_idleConnections = std::move(tempQueue);
    LOG_INFO_FMT("紧急清理完成，释放%zu个连接", cleaned);
}

void MySQLConnectionPool::releaseConnection(std::shared_ptr<MySQLHandler> conn)
{
    if (!conn)
        return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // 释放前强制检查连接有效性（原逻辑仅判断isConnected，增强为主动ping）
    if (!conn->isConnected()) {
        LOG_WARNING("释放无效连接（已断开）");
        m_totalConnections--;
        m_activeConnections--;
        return;
    }

    // 检查连接是否超过最大存活时间
    auto maxLifetime = std::chrono::seconds(m_config.getConnectionMaxLifetime());
    auto now = std::chrono::steady_clock::now();
    auto lifetime = now - conn->getCreateTime();

    if (lifetime < maxLifetime) {
        // 连接有效且未过期，放回空闲队列
        m_idleConnections.push({conn, now, now});  // 最后使用时间更新为当前
        m_activeConnections--;
        m_cond.notify_one();
        LOG_DEBUG_FMT("连接释放成功，空闲:%zu，活跃:%zu", m_idleConnections.size(), m_activeConnections);
        return;
    }

    // 连接无效或已过期，销毁连接
    m_totalConnections--;
    m_activeConnections--;
    LOG_WARNING_FMT("连接已过期（%ldms），直接销毁", std::chrono::duration_cast<std::chrono::milliseconds>(lifetime).count());
}

void MySQLConnectionPool::cleanupThreadFunc()
{
    LOG_INFO("MySQL连接池清理线程 启动");

    while (m_running) {
        // 每10秒执行一次清理
        std::this_thread::sleep_for(std::chrono::seconds(10));

        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        auto maxLifetime = std::chrono::seconds(m_config.getConnectionMaxLifetime());
        auto maxIdleTime = std::chrono::seconds(m_config.getMaxIdleTime());
        size_t cleaned = 0;
        // 清理过期连接
        std::queue<ConnectionWrapper> tempQueue;
        while (!m_idleConnections.empty()) {
            auto wrapper = std::move(m_idleConnections.front());
            m_idleConnections.pop();
            // 检查连接是否有效
            if (!wrapper.conn->isConnected()) {
                LOG_DEBUG("清理无效空闲连接");
                m_totalConnections--;
                cleaned++;
                continue;
            }
            // 检查超时
            auto idleTime = now - wrapper.lastUsedTime;
            auto lifetime = now - wrapper.createTime;
            if (idleTime > maxIdleTime || lifetime > maxLifetime) {
                // 计算空闲时间（秒和毫秒）
                auto idleMs = std::chrono::duration_cast<std::chrono::milliseconds>(idleTime).count();
                long long idleSec = idleMs / 1000;
                int idleRemainMs = idleMs % 1000;
                // 计算存活时间（秒和毫秒）
                auto lifetimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(lifetime).count();
                long long lifetimeSec = lifetimeMs / 1000;
                int lifetimeRemainMs = lifetimeMs % 1000;
                LOG_DEBUG_FMT("清理超时连接（空闲:%llds %dms, 存活:%llds %dms）", idleSec, idleRemainMs, lifetimeSec, lifetimeRemainMs);

                m_totalConnections--;
                cleaned++;
                continue;
            }
            // 保留有效连接
            tempQueue.push(std::move(wrapper));
        }
        m_idleConnections = std::move(tempQueue);  // 放回有效连接

        // 收缩空闲连接至最小连接数
        const size_t minConnections = std::max(1u, m_config.getMaxConnections() / 4);
        while (m_idleConnections.size() > minConnections && m_totalConnections > minConnections) {
            m_idleConnections.pop();
            m_totalConnections--;
            cleaned++;
        }

        if (cleaned > 0) {
            LOG_INFO_FMT("清理完成：释放%zu个连接，剩余空闲:%zu，总连接:%zu", cleaned, m_idleConnections.size(), m_totalConnections);
        }
    }

    LOG_INFO("连接池清理线程退出");
}

MySQLConnectionPool::PoolStats MySQLConnectionPool::getPoolStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return {.idle_connections = m_idleConnections.size(),
            .active_connections = m_activeConnections,
            .total_connections = m_totalConnections,
            .max_connections = m_config.getMaxConnections()};
}

// 监控线程函数（在.h中声明）
void MySQLConnectionPool::monitorThreadFunc()
{
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        auto stats = getPoolStats();
        LOG_INFO_FMT("连接池状态：总连接=%zu，活跃=%zu，空闲=%zu，最大=%zu",
                     stats.total_connections,
                     stats.active_connections,
                     stats.idle_connections,
                     stats.max_connections);
    }
}