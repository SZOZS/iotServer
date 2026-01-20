#pragma once
#ifndef CONFIG_MYSQL_H
#define CONFIG_MYSQL_H

#include <string>
#include <vector>

#include "ConfigGlobal.h"

class ConfigMySQL {
 private:
  ConfigMySQL() {
    auto& globalMysql = ConfigGlobal::getInstance().getConfigMySQL();
    m_host = globalMysql.host;              // ip
    m_user = globalMysql.user;              // 账号
    m_password = globalMysql.password;      // 密码
    m_database = globalMysql.database;      // 数据库
    m_port = globalMysql.port;              // 端口
    m_timeout = globalMysql.timeout;        // 连接超时
    m_softDelete = globalMysql.softDelete;  // 安全删除开启
    m_maxRetries = globalMysql.maxRetries;  // 最大重试次数
    m_retryDelay = globalMysql.retryDelay;  // 重连延迟（毫秒）

    /*
    场景	服务器配置（CPU / 内存）	推荐 max_connections	备注 单个连接按 20MB 算
    - 小型应用 | 2C /4GB ~ 8GB | 200 ~ 500 | 并发低（峰值 < 200），短连接为主，无需过高配置。
    - 中型应用 | 4C /16GB~32GB | 500 ~1000 | 并发中等（峰值 300~800），建议配合连接池使用，减少短连接开销。
    - 大型应用 | 8C+/64GB+     | 1000~2000 | 需优化 MySQL 线程模型（如启用 thread_pool 插件）、分库分表、读写分离，避免单实例依赖过高连接数。
    */
    m_maxConnections = globalMysql.maxConnections;  // 最大连接数阈值

    /*
    - 应用使用连接池（复用连接）      ｜1800 ~ 7200秒  ｜30 分钟～2 小时。连接池会自动管理连接，避免频繁重建（重建连接耗时）。
    - 无连接池的短连接应用           ｜300 ~ 600秒    ｜5 ~ 10 分钟。防止异常未关闭的连接长期占用资源。
    - 存在长闲置周期的业务（如定时任务）｜3600 ~ 10800秒 ｜1 ~ 3 小时。需大于业务的最大闲置周期（如定时任务每 2 小时执行一次，则设为 3 小时）。
    - 高并发、资源紧张的服务器        ｜180 ~ 300秒    ｜3 ~ 5 分钟。快速回收闲置连接，释放内存和连接数配额。
    */
    m_connectionMaxLifetime = globalMysql.connectionMaxLifetime;  // 连接最大存活时间（秒），超过则视为超时
    m_maxIdleTime = globalMysql.maxIdleTime;  // 最大空闲时间（秒）
  }

  ~ConfigMySQL() = default;

  std::string m_host;
  std::string m_user;
  std::string m_password;
  std::string m_database;
  unsigned int m_port;
  unsigned int m_timeout;
  bool m_softDelete;
  unsigned int m_maxRetries;
  unsigned int m_retryDelay;
  unsigned int m_maxConnections;
  unsigned int m_connectionMaxLifetime;
  unsigned int m_maxIdleTime;

 public:
  // 禁止拷贝构造和赋值
  ConfigMySQL(const ConfigMySQL&) = delete;
  ConfigMySQL& operator=(const ConfigMySQL&) = delete;

  //   获取单例实现
  static ConfigMySQL& getInstance() {
    static ConfigMySQL instance;
    return instance;
  }

  //   获取配置
  std::string getHost() const { return m_host; }
  std::string getUser() const { return m_user; }
  std::string getPassword() const { return m_password; }
  std::string getDatabase() const { return m_database; }
  unsigned int getPort() const { return m_port; }
  unsigned int getTimeout() const { return m_timeout; }
  bool isSoftDeleteEnabled() const { return m_softDelete; }
  unsigned int getMaxRetries() const { return m_maxRetries; }
  unsigned int getRetryDelay() const { return m_retryDelay; }
  unsigned int getMaxConnections() const { return m_maxConnections; }
  unsigned int getConnectionMaxLifetime() const { return m_connectionMaxLifetime; }
  unsigned int getMaxIdleTime() const { return m_maxIdleTime; }
};

#endif  // CONFIG_MYSQL_H