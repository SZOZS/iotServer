#pragma once
#ifndef MYSQL_HANDLER_H
#define MYSQL_HANDLER_H

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <mysql/mysql.h>
#include <string>
#include <vector>

#include "../../configs/ConfigMySQL.h"

class MySQLHandler
{
public:
    // 构造函数：通过连接参数初始化,通过配置对象初始化
    MySQLHandler(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port = 3306);
    explicit MySQLHandler(const ConfigMySQL& config);

    // 禁止拷贝
    MySQLHandler(const MySQLHandler&) = delete;
    MySQLHandler& operator=(const MySQLHandler&) = delete;

    // 支持移动
    MySQLHandler(MySQLHandler&&) noexcept;
    MySQLHandler& operator=(MySQLHandler&&) noexcept;

    ~MySQLHandler();  // 析构函数

    bool isConnected() const;  // 连接状态检查
    // 原始SQL查询
    std::vector<std::map<std::string, std::string>> query(const std::string& sql);
    // 参数化查询
    std::vector<std::map<std::string, std::string>> queryWithParams(const std::string& sql, const std::vector<std::string>& params);
    // 单条记录查询
    std::map<std::string, std::string> queryOne(const std::string& table,
                                                const std::vector<std::string>& fields,
                                                const std::vector<std::map<std::string, std::string>>& conditions);
    std::map<std::string, std::string> queryOneMax(const std::string& table,
                                                   const std::vector<std::string>& fields,
                                                   const std::vector<std::vector<std::string>>& conditions,
                                                   const std::map<std::string, std::string>& order_by = {});
    // 多条记录查询
    std::vector<std::map<std::string, std::string>> queryMulti(const std::string& table,
                                                               const std::vector<std::string>& fields,
                                                               const std::vector<std::map<std::string, std::string>>& conditions = {});
    // 插入操作
    bool insertOne(const std::string& table, const std::map<std::string, std::string>& data);
    int insertMulti(const std::string& table, const std::vector<std::string>& fields, const std::vector<std::map<std::string, std::string>>& datas);
    // 更新操作
    bool update(const std::string& table, const std::map<std::string, std::string>& data, const std::string& condition);
    // SQL执行（带结果集）
    bool executeSQL(const std::string& sql, MYSQL_RES** result = nullptr);
    // 参数化执行（无结果集）
    bool executeWithParams(const std::string& sql, const std::vector<std::string>& params);
    // 删除操作
    bool remove(const std::string& table, const std::string& condition);
    // 获取最后插入ID
    uint64_t getLastInsertId() const;
    // 获取影响行数
    unsigned long getAffectedRows() const;
    // 获取连接创建时间
    std::chrono::steady_clock::time_point getCreateTime() const { return m_createTime; }
    bool ping() const;

private:
    MYSQL* m_conn;                                       // MySQL连接句柄
    mutable std::recursive_mutex m_mutex;                // 线程安全锁
    bool m_connected;                                    // 连接状态
    std::string m_database;                              // 当前数据库名
    std::chrono::steady_clock::time_point m_createTime;  // 记录连接创建时间

    bool insert(const std::string& table, const std::map<std::string, std::string>& data);
    // 初始化连接
    void initConnection(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port);
    // 字符串转义（防注入）
    std::string escapeString(const std::string& str);
    // 清理连接资源
    void cleanup();
};

#endif  // MYSQL_HANDLER_H