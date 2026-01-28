#include <cassert>
#include <sstream>

#include "../../logs/Logger.h"

#include "MySQLHandler.h"

MySQLHandler::MySQLHandler(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port)
    : m_conn(nullptr), m_connected(false), m_database(database), m_createTime(std::chrono::steady_clock::now())
{
    initConnection(host, user, password, database, port);
}

MySQLHandler::MySQLHandler(const ConfigMySQL& config)
    : m_conn(nullptr), m_connected(false), m_database(config.getDatabase()), m_createTime(std::chrono::steady_clock::now())
{
    initConnection(config.getHost(), config.getUser(), config.getPassword(), config.getDatabase(), config.getPort());
}

MySQLHandler::MySQLHandler(MySQLHandler&& other) noexcept : m_conn(other.m_conn), m_connected(other.m_connected), m_database(std::move(other.m_database))
{
    other.m_conn = nullptr;
    other.m_connected = false;
}

MySQLHandler& MySQLHandler::operator=(MySQLHandler&& other) noexcept
{
    if (this != &other) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        cleanup();
        m_conn = other.m_conn;
        m_connected = other.m_connected;
        m_database = std::move(other.m_database);
        other.m_conn = nullptr;
        other.m_connected = false;
    }
    return *this;
}

MySQLHandler::~MySQLHandler()
{
    cleanup();
}

void MySQLHandler::cleanup()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_conn) {
        mysql_close(m_conn);
        m_conn = nullptr;
        m_connected = false;
        // LOG_DEBUG("MySQL 连接已关闭");
    }
}

void MySQLHandler::initConnection(const std::string& host, const std::string& user, const std::string& password, const std::string& database, unsigned int port)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_conn = mysql_init(nullptr);
    if (!m_conn) {
        LOG_ERROR("MySQL 初始化失败");
        return;
    }

    // 设置超时
    unsigned int timeout = 30;  // 30秒超时
    mysql_options(m_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // 设置字符集
    if (mysql_options(m_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4") != 0) {
        LOG_WARNING_FMT("设置字符集失败: %s", mysql_error(m_conn));
    }

    // 建立连接
    // LOG_DEBUG_FMT("%s | %s | %s | %s | %d", host.c_str(), user.c_str(), password.c_str(), database.c_str(), port);
    if (!mysql_real_connect(m_conn, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0)) {
        LOG_ERROR_FMT("MySQL 连接失败: %s", mysql_error(m_conn));
        cleanup();
    } else {
        m_connected = true;
        // LOG_DEBUG_FMT("MySQL 连接成功 (版本: %s, 数据库: %s)", mysql_get_server_info(m_conn), database.c_str());
    }
}

bool MySQLHandler::isConnected() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_connected && m_conn && mysql_ping(m_conn) == 0;
}

std::vector<std::map<std::string, std::string>> MySQLHandler::query(const std::string& sql)
{
    std::vector<std::map<std::string, std::string>> result;
    MYSQL_RES* res = nullptr;

    if (!executeSQL(sql, &res))
        return result;
    if (!res)
        return result;

    try {
        MYSQL_FIELD* fields = mysql_fetch_fields(res);
        unsigned int numFields = mysql_num_fields(res);
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {
            std::map<std::string, std::string> record;
            unsigned long* lengths = mysql_fetch_lengths(res);

            for (unsigned int i = 0; i < numFields; ++i) {
                if (row[i]) {
                    record[fields[i].name] = std::string(row[i], lengths[i]);
                } else {
                    record[fields[i].name] = "";
                }
            }
            result.push_back(record);
        }
    } catch (...) {
        LOG_ERROR("处理查询结果时出错");
    }

    mysql_free_result(res);
    return result;
}

std::vector<std::map<std::string, std::string>> MySQLHandler::queryWithParams(const std::string& sql, const std::vector<std::string>& params)
{
    std::vector<std::map<std::string, std::string>> result;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (!isConnected()) {
        LOG_ERROR("参数化查询的连接无效");
        return result;
    }

    // 打印参数化SQL模板和参数列表
    LOG_DEBUG_FMT("参数化SQL模板: %s", sql.c_str());
    std::string paramsStr;
    for (size_t i = 0; i < params.size(); ++i) {
        paramsStr += (i > 0 ? ", " : "") + std::string("'") + params[i] + std::string("'");
    }
    LOG_DEBUG_FMT("参数: [%s]", paramsStr.c_str());
    // 模拟拼接带参数的完整SQL(用于调试)
    auto escapeParam = [](const std::string& param) {
        std::string escaped;
        for (char c : param) {
            if (c == '\'')
                escaped += "''";  // 转义单引号
            else
                escaped += c;
        }
        return escaped;
    };
    std::string sqlWithParams;
    size_t paramIdx = 0;
    for (char c : sql) {
        if (c == '?' && paramIdx < params.size()) {
            sqlWithParams += "'" + escapeParam(params[paramIdx]) + "'";
            paramIdx++;
        } else {
            sqlWithParams += c;
        }
    }
    LOG_DEBUG_FMT("带值的参数化SQL: %s", sqlWithParams.c_str());

    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) {
        LOG_ERROR_FMT("语句初始化失败: %s", mysql_error(m_conn));
        return result;
    }

    // 预处理语句
    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR_FMT("准备失败: %s (SQL: %s)", mysql_stmt_error(stmt), sql.c_str());
        mysql_stmt_close(stmt);
        return result;
    }

    // 参数数量校验
    unsigned long paramCount = mysql_stmt_param_count(stmt);
    if (paramCount != params.size()) {
        LOG_ERROR_FMT("参数计数不匹配：必需 %lu, 实际 %zu", paramCount, params.size());
        mysql_stmt_close(stmt);
        return result;
    }

    // 绑定参数
    std::vector<MYSQL_BIND> binds(paramCount);
    std::vector<unsigned long> lengths(paramCount);
    std::vector<char*> paramData(paramCount);

    for (size_t i = 0; i < paramCount; ++i) {
        paramData[i] = const_cast<char*>(params[i].c_str());
        lengths[i] = params[i].size();
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = paramData[i];
        binds[i].buffer_length = lengths[i];
        binds[i].length = &lengths[i];
    }

    if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
        LOG_ERROR_FMT("参数绑定失败: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return result;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt) != 0) {
        LOG_ERROR_FMT("执行失败: %s (SQL: %s)", mysql_stmt_error(stmt), sql.c_str());
        mysql_stmt_close(stmt);
        return result;
    }

    // 处理结果集
    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt);
    if (!meta) {
        mysql_stmt_close(stmt);
        return result;
    }

    unsigned int numFields = mysql_num_fields(meta);
    std::vector<std::string> fieldNames;
    MYSQL_FIELD* fields = mysql_fetch_fields(meta);
    for (unsigned int i = 0; i < numFields; ++i) {
        fieldNames.push_back(fields[i].name);
    }

    // 绑定结果缓冲区
    std::vector<MYSQL_BIND> resultBinds(numFields);
    std::vector<char> buffers(numFields * 4096);  // 4KB per field
    std::vector<unsigned long> resultLengths(numFields);
    std::vector<char> isNull(numFields, 0);

    for (unsigned int i = 0; i < numFields; ++i) {
        resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
        resultBinds[i].buffer = &buffers[i * 4096];
        resultBinds[i].buffer_length = 4096;
        resultBinds[i].length = &resultLengths[i];
        resultBinds[i].is_null = reinterpret_cast<bool*>(&isNull[i]);
    }

    if (mysql_stmt_bind_result(stmt, resultBinds.data()) != 0) {
        LOG_ERROR_FMT("结果绑定失败: %s", mysql_stmt_error(stmt));
        mysql_free_result(meta);
        mysql_stmt_close(stmt);
        return result;
    }

    // 提取结果
    int fetchResult;
    while ((fetchResult = mysql_stmt_fetch(stmt)) == 0) {
        std::map<std::string, std::string> row;
        for (unsigned int i = 0; i < numFields; ++i) {
            if (*resultBinds[i].is_null) {
                row[fieldNames[i]] = "";
            } else {
                row[fieldNames[i]] = std::string(static_cast<char*>(resultBinds[i].buffer), resultLengths[i]);
            }
        }
        result.push_back(row);
    }

    if (fetchResult != MYSQL_NO_DATA) {
        LOG_ERROR_FMT("获取失败: %s", mysql_stmt_error(stmt));
    }

    mysql_free_result(meta);
    mysql_stmt_close(stmt);
    return result;
}

std::map<std::string, std::string> MySQLHandler::queryOne(const std::string& table,
                                                          const std::vector<std::string>& fields,
                                                          const std::vector<std::map<std::string, std::string>>& conditions)
{
    std::map<std::string, std::string> result;
    if (table.empty() || fields.empty()) {
        LOG_ERROR("表名或字段不能为空");
        return result;
    }

    try {
        std::stringstream sql;
        std::vector<std::string> params;

        // 构建SELECT子句
        sql << "SELECT ";
        for (size_t i = 0; i < fields.size(); ++i) {
            sql << (i > 0 ? ", " : "") << "`" << fields[i] << "`";
        }

        // 构建FROM子句
        sql << " FROM `" << table << "`";

        // 构建WHERE子句
        if (!conditions.empty()) {
            sql << " WHERE ";
            size_t condCount = 0;

            for (const auto& condMap : conditions) {
                for (const auto& condPair : condMap) {
                    const std::string& field = condPair.first;
                    const std::string& value = condPair.second;

                    if (condCount > 0)
                        sql << " AND ";

                    if (value == "NULL") {
                        sql << "`" << field << "` IS NULL";
                    } else {
                        sql << "`" << field << "` = ?";
                        params.push_back(value);
                    }
                    condCount++;
                }
            }
        }

        sql << " LIMIT 1";

        // 打印自动生成的SQL模板和参数
        LOG_DEBUG_FMT("自动生成 queryOne SQL: %s", sql.str().c_str());
        std::string paramsStr;
        for (size_t i = 0; i < params.size(); ++i) {
            paramsStr += (i > 0 ? ", " : "") + std::string("'") + params[i] + std::string("'");
        }
        LOG_DEBUG_FMT("queryOne 参数: [%s]", paramsStr.c_str());

        auto queryResult = queryWithParams(sql.str(), params);
        if (!queryResult.empty()) {
            result = queryResult[0];
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("单条数据查询失败: %s", e.what());
    }

    return result;
}

std::vector<std::map<std::string, std::string>> MySQLHandler::queryMulti(const std::string& table,
                                                                         const std::vector<std::string>& fields,
                                                                         const std::vector<std::map<std::string, std::string>>& conditions)
{
    std::vector<std::map<std::string, std::string>> results;
    if (table.empty() || fields.empty()) {
        LOG_ERROR("表名或字段不能为空");
        return results;
    }

    try {
        std::stringstream sql;
        std::vector<std::string> params;

        // 构建SELECT子句
        sql << "SELECT ";
        for (size_t i = 0; i < fields.size(); ++i) {
            sql << (i > 0 ? ", " : "") << "`" << fields[i] << "`";
        }

        // 构建FROM子句
        sql << " FROM `" << table << "`";

        // 构建WHERE子句
        if (!conditions.empty()) {
            sql << " WHERE ";
            size_t condCount = 0;

            for (const auto& condMap : conditions) {
                for (const auto& condPair : condMap) {
                    const std::string& field = condPair.first;
                    const std::string& value = condPair.second;

                    if (condCount > 0)
                        sql << " AND ";

                    if (value == "NULL") {
                        sql << "`" << field << "` IS NULL";
                    } else {
                        sql << "`" << field << "` = ?";
                        params.push_back(value);
                    }
                    condCount++;
                }
            }
        }

        // 打印自动生成的SQL和参数
        LOG_DEBUG_FMT("自动生成 queryMulti SQL: %s", sql.str().c_str());
        std::string paramsStr;
        for (size_t i = 0; i < params.size(); ++i) {
            paramsStr += (i > 0 ? ", " : "") + std::string("'") + params[i] + std::string("'");
        }
        LOG_DEBUG_FMT("queryMulti 参数: [%s]", paramsStr.c_str());

        results = queryWithParams(sql.str(), params);
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("多条查询失败: %s", e.what());
    }

    return results;
}

bool MySQLHandler::insert(const std::string& table, const std::map<std::string, std::string>& data)
{
    if (table.empty() || data.empty()) {
        LOG_ERROR("表名或数据不能为空");
        return false;
    }

    std::stringstream sql;
    sql << "INSERT INTO `" << table << "` (";

    // 字段名
    size_t i = 0;
    for (const auto& pair : data) {
        if (i++ > 0)
            sql << ", ";
        sql << "`" << pair.first << "`";
    }

    // 值
    sql << ") VALUES (";
    i = 0;
    for (const auto& pair : data) {
        if (i++ > 0)
            sql << ", ";
        sql << "'" << escapeString(pair.second) << "'";
    }
    sql << ")";

    return executeSQL(sql.str());
}

bool MySQLHandler::insertOne(const std::string& table, const std::map<std::string, std::string>& data)
{
    return insert(table, data);
}

int MySQLHandler::insertMulti(const std::string& table, const std::vector<std::string>& fields, const std::vector<std::map<std::string, std::string>>& datas)
{
    if (table.empty() || fields.empty() || datas.empty()) {
        LOG_ERROR("表名、字段或数据不能为空");
        return 0;
    }

    std::stringstream sql;
    sql << "INSERT INTO `" << table << "` (";

    // 字段名
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0)
            sql << ", ";
        sql << "`" << fields[i] << "`";
    }
    sql << ") VALUES ";

    // 批量值
    for (size_t i = 0; i < datas.size(); ++i) {
        if (i > 0)
            sql << ", ";
        sql << "(";

        for (size_t j = 0; j < fields.size(); ++j) {
            if (j > 0)
                sql << ", ";
            auto it = datas[i].find(fields[j]);
            if (it != datas[i].end()) {
                sql << "'" << escapeString(it->second) << "'";
            } else {
                sql << "NULL";
            }
        }
        sql << ")";
    }

    LOG_ERROR_FMT("SQL : %s", sql.str().c_str());
    if (executeSQL(sql.str())) {
        return static_cast<int>(getAffectedRows());
    }
    return 0;
}

bool MySQLHandler::update(const std::string& table, const std::map<std::string, std::string>& data, const std::string& condition)
{
    if (table.empty() || data.empty() || condition.empty()) {
        LOG_ERROR("表名、数据或条件不能为空");
        return false;
    }

    std::stringstream sql;
    sql << "UPDATE `" << table << "` SET ";

    size_t i = 0;
    for (const auto& pair : data) {
        if (i++ > 0)
            sql << ", ";
        sql << "`" << pair.first << "` = '" << escapeString(pair.second) << "'";
    }

    sql << " WHERE " << condition;
    return executeSQL(sql.str());
}

bool MySQLHandler::executeSQL(const std::string& sql, MYSQL_RES** result)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!isConnected()) {
        LOG_ERROR("未连接到MySQL服务器");
        return false;
    }

    LOG_DEBUG_FMT("待执行 SQL: %s", sql.c_str());

    if (mysql_real_query(m_conn, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR_FMT("SQL语句执行失败: %s (SQL: %s)", mysql_error(m_conn), sql.c_str());
        return false;
    }

    if (result) {
        *result = mysql_store_result(m_conn);
        if (!*result && mysql_field_count(m_conn) > 0) {
            LOG_ERROR_FMT("获取结果失败: %s (SQL: %s)", mysql_error(m_conn), sql.c_str());
            return false;
        }
    }

    return true;
}

bool MySQLHandler::executeWithParams(const std::string& sql, const std::vector<std::string>& params)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!isConnected()) {
        LOG_ERROR("参数化执行的连接无效");
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(m_conn);
    if (!stmt) {
        LOG_ERROR_FMT("语句初始化失败: %s", mysql_error(m_conn));
        return false;
    }

    if (mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) != 0) {
        LOG_ERROR_FMT("准备失败: %s (SQL: %s)", mysql_stmt_error(stmt), sql.c_str());
        mysql_stmt_close(stmt);
        return false;
    }

    unsigned long paramCount = mysql_stmt_param_count(stmt);
    if (paramCount != params.size()) {
        LOG_ERROR_FMT("参数计数不匹配：必需 %lu, 实际 %zu", paramCount, params.size());
        mysql_stmt_close(stmt);
        return false;
    }

    std::vector<MYSQL_BIND> binds(paramCount);
    std::vector<unsigned long> lengths(paramCount);
    std::vector<char*> paramData(paramCount);

    for (size_t i = 0; i < paramCount; ++i) {
        paramData[i] = const_cast<char*>(params[i].c_str());
        lengths[i] = params[i].size();
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = paramData[i];
        binds[i].buffer_length = lengths[i];
        binds[i].length = &lengths[i];
    }

    if (mysql_stmt_bind_param(stmt, binds.data()) != 0) {
        LOG_ERROR_FMT("参数绑定失败: %s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return false;
    }

    bool success = (mysql_stmt_execute(stmt) == 0);
    if (!success) {
        LOG_ERROR_FMT("执行失败: %s (SQL: %s)", mysql_stmt_error(stmt), sql.c_str());
    }

    mysql_stmt_close(stmt);
    return success;
}

bool MySQLHandler::remove(const std::string& table, const std::string& condition)
{
    if (table.empty() || condition.empty()) {
        LOG_ERROR("表名或条件不能为空");
        return false;
    }

    std::string sql = "DELETE FROM `" + table + "` WHERE " + condition;
    return executeSQL(sql);
}

uint64_t MySQLHandler::getLastInsertId() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_conn ? mysql_insert_id(m_conn) : 0;
}

unsigned long MySQLHandler::getAffectedRows() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_conn ? mysql_affected_rows(m_conn) : 0;
}

std::string MySQLHandler::escapeString(const std::string& str)
{
    if (str.empty())
        return "";

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_conn)
        return "";

    std::unique_ptr<char[]> escaped(new char[str.size() * 2 + 1]);
    mysql_real_escape_string(m_conn, escaped.get(), str.c_str(), str.size());
    return std::string(escaped.get());
}

bool MySQLHandler::ping() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_conn)
        return false;                // 连接句柄为空则返回失败
    return mysql_ping(m_conn) == 0;  // 调用MySQL库的ping方法
}

std::map<std::string, std::string> MySQLHandler::queryOneMax(const std::string& table,
                                                             const std::vector<std::string>& fields,
                                                             const std::vector<std::vector<std::string>>& conditions,
                                                             const std::map<std::string, std::string>& order_by)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!isConnected()) {
        LOG_ERROR("数据库连接无效,无法执行queryOneMax");
        return {};
    }

    // 构建SQL基础部分
    std::string sql = "SELECT ";
    if (fields.empty()) {
        sql += "*";  // 默认查询所有字段
    } else {
        for (size_t i = 0; i < fields.size(); ++i) {
            sql += (i > 0 ? ", " : "") + escapeString(fields[i]);
        }
    }
    sql += " FROM " + escapeString(table);

    // 构建WHERE条件(支持两种格式：等于条件[字段,值] 和 比较条件[字段,运算符,值])
    if (!conditions.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < conditions.size(); ++i) {
            const auto& cond = conditions[i];
            // 校验条件格式(必须是2个或3个元素)
            if (cond.size() != 2 && cond.size() != 3) {
                LOG_ERROR_FMT("条件格式错误(索引%zu),必须包含2个或3个元素", i);
                return {};
            }

            std::string field = escapeString(cond[0]);
            std::string op = (cond.size() == 2) ? "=" : cond[1];  // 2元素默认是等于
            std::string value = cond.back();

            // 转义值(防SQL注入,保留数字和时间戳原样)
            std::string escapedValue;
            if (value.empty()) {
                escapedValue = "NULL";
            } else if (isdigit(value[0]) && value.find_first_not_of("0123456789") == std::string::npos) {
                escapedValue = value;  // 数字/时间戳不转义
            } else {
                escapedValue = "'" + escapeString(value) + "'";  // 字符串加单引号
            }

            sql += (i > 0 ? " AND " : "") + field + " " + op + " " + escapedValue;
        }
    }

    // 构建排序(默认按id降序,确保取最大记录)
    std::map<std::string, std::string> finalOrder = order_by;
    if (finalOrder.empty()) {
        finalOrder["id"] = "DESC";  // 默认按id降序
    }
    sql += " ORDER BY ";
    size_t i = 0;
    for (const auto& orderItem : finalOrder) {
        // 提取字段名和排序方向（替代结构化绑定）
        const std::string& field = orderItem.first;
        const std::string& direction = orderItem.second;
        // 原有业务逻辑完全保留
        std::string dir = (direction == "DESC" ? "DESC" : "ASC");
        sql += (i > 0 ? ", " : "") + escapeString(field) + " " + dir;
        i++;
    }

    // 只取一条记录
    sql += " LIMIT 1";

    LOG_INFO_FMT("%s", sql.c_str());
    // 执行查询并返回第一条结果
    std::vector<std::map<std::string, std::string>> result = query(sql);
    return result.empty() ? std::map<std::string, std::string>() : result[0];
}
