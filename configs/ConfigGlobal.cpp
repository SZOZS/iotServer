#include <fstream>
#include <iostream>

#include "../logs/Logger.h"

#include "ConfigGlobal.h"

void ConfigGlobal::loadConfig(const std::string& filePath)
{
    try {
        LOG_DEBUG_DELAY("[config] ==> loding ...");
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) {
            LOG_ERROR_FMT_DELAY("配置文件 %s 打开失败 %s", filePath.c_str(), strerror(errno));
            return;
        }
        if (ifs.peek() == std::ifstream::traits_type::eof()) {
            LOG_ERROR_FMT_DELAY("配置文件 %s 是空的", filePath.c_str());
            return;
        }

        json j;
        ifs >> j;

        // 解析Hex配置
        if (j.contains("hex")) {
            LOG_DEBUG_DELAY("[hex] ==> loding ...");
            auto& hex = j["hex"];
            m_configHex.hexUppercase = hex["uppercase"];   // 使用大写
            m_configHex.hexWithSpace = hex["with_space"];  // 使用空格
            LOG_DEBUG_DELAY("[hex] √ ");
        } else {
            LOG_ERROR_FMT_DELAY("[ERROR]==>%s-hex", filePath.c_str());
        }

        // 解析日志配置
        if (j.contains("log")) {
            LOG_DEBUG_DELAY("[log] ==> loding ...");
            auto& log = j["log"];
            m_configLog.filePath = log["file_path"];  // 读取日志文件路径
            LOG_DEBUG_DELAY("[log] √ ");
        } else {
            LOG_ERROR_FMT_DELAY("[FAIL]==>%s-log (use default: app.log)", filePath.c_str());
        }

        // 解析MySQL配置
        if (j.contains("mysql")) {
            LOG_DEBUG_DELAY("[mysql] ==> loding ...");
            auto& mysql = j["mysql"];
            m_configMySQL.host = mysql["host"];                                      // 连接地址
            m_configMySQL.user = mysql["user"];                                      // 账号
            m_configMySQL.password = mysql["password"];                              // 密码
            m_configMySQL.database = mysql["database"];                              // 数据库
            m_configMySQL.port = mysql["port"];                                      // 端口
            m_configMySQL.timeout = mysql["timeout"];                                // 连接超时
            m_configMySQL.softDelete = mysql["soft_delete"];                         // 安全删除开启
            m_configMySQL.maxRetries = mysql["max_retries"];                         // 最大重试次数
            m_configMySQL.retryDelay = mysql["retry_delay"];                         // 重连延迟（毫秒）
            m_configMySQL.maxConnections = mysql["max_connections"];                 // 最大连接数阈值
            m_configMySQL.connectionMaxLifetime = mysql["connection_max_lifetime"];  // 连接最大存活时间（秒），超过则视为超时
            m_configMySQL.maxIdleTime = mysql["max_idle_time"];                      // 最大空闲时间（默认300秒）
            LOG_DEBUG_DELAY("[mysql] √ ");
        } else {
            LOG_ERROR_FMT_DELAY("[FAIL]==>%s-mysql", filePath.c_str());
        }

        // 解析Redis配置
        if (j.contains("redis")) {
            auto& redis = j["redis"];
            LOG_DEBUG_DELAY("[redis] ==> loding ...");
            m_configRedis.host = redis["host"];                                      // 连接地址
            m_configRedis.port = redis["port"];                                      // 端口
            m_configRedis.password = redis["password"];                              // 密码
            m_configRedis.db = redis["db"];                                          // 数据库
            m_configRedis.timeout = redis["timeout"];                                // 连接超时
            m_configRedis.maxRetries = redis["max_retries"];                         // 最大重试次数
            m_configRedis.retryDelay = redis["retry_delay"];                         // 重连延迟（毫秒）
            m_configRedis.maxConnections = redis["max_connections"];                 // 最大连接数阈值
            m_configRedis.connectionMaxLifetime = redis["connection_max_lifetime"];  // 连接最大存活时间（秒），超过则视为超时
            m_configRedis.maxIdleTime = redis["max_idle_time"];                      // 最大空闲时间（默认300秒）
            LOG_DEBUG_DELAY("[redis] √ ");
        } else {
            LOG_ERROR_FMT_DELAY("[FAIL]==>%s-redis", filePath.c_str());
        }

        // 解析PHP Push IP白名单配置
        if (j.contains("php_push")) {
            LOG_DEBUG_DELAY("[php_push] ==> loding ...");
            auto& php_push = j["php_push"];
            for (const auto& ip : php_push["ip_whitelist"]) {
                m_configPhpPush.ip_whitelist.insert(ip.get<std::string>());
            }
            LOG_DEBUG_DELAY("[php_push] √ ");
        } else {
            LOG_ERROR_FMT_DELAY("[FAIL]==>%s-php_push", filePath.c_str());
        }

        if (j.contains("port_protocol") && j["port_protocol"].is_object()) {
            LOG_DEBUG_DELAY("[port_protocol] ==> loding ...");
            const json& port_protocol_obj = j["port_protocol"];

            // 遍历所有通信大类（MQTT/CoAP/HTTP/HTTPS/WebSocket等）
            for (auto it = port_protocol_obj.begin(); it != port_protocol_obj.end(); ++it) {
                const std::string& group_name = it.key();  // 通信大类名称（如CoAP）
                const json& item_arr = it.value();         // 大类下的子协议数组

                // 校验当前大类的值是否为数组
                if (!item_arr.is_array()) {
                    throw std::runtime_error("Config error: " + group_name + " in port_protocol is not an array");
                }

                std::vector<PortProtocolItem> group_items;
                // 遍历当前大类下的所有子协议项
                for (size_t i = 0; i < item_arr.size(); ++i) {
                    const json& item = item_arr[i];
                    PortProtocolItem pp_item;

                    // 解析protocol字段
                    if (!item.contains("protocol") || !item["protocol"].is_string()) {
                        throw std::runtime_error("Config error: protocol missing or not string in " + group_name + " at index " + std::to_string(i));
                    }
                    pp_item.protocol = item["protocol"].get<std::string>();

                    // 解析ports字段（转换为short，校验范围）
                    if (!item.contains("port") || !item["port"].is_number_integer()) {
                        throw std::runtime_error("Config error: port missing or not integer in " + group_name + " at index " + std::to_string(i));
                    }
                    int port_val = item["port"].get<int>();
                    if (port_val < 0 || port_val > SHRT_MAX) {
                        throw std::out_of_range("Port value " + std::to_string(port_val) + " out of short range in " + group_name + " at index " +
                                                std::to_string(i));
                    }
                    pp_item.port = static_cast<short>(port_val);

                    group_items.push_back(pp_item);
                }

                // 将当前大类的子协议列表存入配置
                m_configPortProtocol.protocolGroups[group_name] = group_items;
            }
            LOG_DEBUG_DELAY("[port_protocol] √ ");
        }
        LOG_DEBUG_DELAY("[config] √ ");
    } catch (const std::exception& e) {
        LOG_ERROR_FMT_DELAY("[ERROR]==>%s：%s", filePath.c_str(), e.what());
    }
}

std::vector<short> ConfigGlobal::getUniquePortsFromConfig()
{
    // 1. 通过单例获取配置实例
    auto& portProtocolConfig = ConfigGlobal::getInstance().getConfigPortProtocol();

    // 2. 去重存储端口
    std::unordered_set<short> port_set;

    // 【关键修改】C++14 遍历 unordered_map 的方式（替代结构化绑定）
    // 遍历所有通信大类（MQTT/CoAP/WebSocket等）
    for (const auto& group_pair : portProtocolConfig.protocolGroups) {
        // group_pair 是 pair<const std::string, std::vector<PortProtocolItem>>
        // const std::string& group_name = group_pair.first;                // 通信大类名称（如CoAP）
        const std::vector<PortProtocolItem>& items = group_pair.second;  // 大类下的子协议列表

        // 遍历当前大类下的所有子协议项
        for (const auto& item : items) {
            port_set.insert(item.port);
        }
    }

    // 3. 转换为vector并排序
    std::vector<short> unique_ports;
    unique_ports.reserve(port_set.size());
    for (short port : port_set) {
        unique_ports.push_back(port);
    }
    std::sort(unique_ports.begin(), unique_ports.end());

    return unique_ports;
}

// === CoAP 协议配置 ===
// 子协议：SD_RTU，端口：8234
// 子协议：SD_YC，端口：8234
// 子协议：HY_LORA，端口：8234
// 子协议：HK_WG，端口：8236
// 子协议：HK_YC，端口：8236
// 子协议：HK_SX，端口：8236
// === WebSocket 协议配置 ===
// 子协议：WEB_SOCKET，端口：8235
// === 所有通信大类配置 ===
// 大类：MQTT，子协议数量：0
// 大类：CoAP，子协议数量：6
// 大类：HTTP/HTTPS，子协议数量：0
// 大类：WebSocket，子协议数量：1
// 大类：MQTT-SN，子协议数量：0
// 大类：LwM2M，子协议数量：0
// 大类：AMQP，子协议数量：0
void ConfigGlobal::logPortProtocolConfig()
{
    // 获取端口协议配置实例（const 引用）
    const auto& portProtocolConfig = ConfigGlobal::getInstance().getConfigPortProtocol();

    // 1. 打印CoAP大类下的配置（改用find()访问const map）
    auto coapIt = portProtocolConfig.protocolGroups.find("CoAP");
    if (coapIt != portProtocolConfig.protocolGroups.end()) {
        const auto& coapItems = coapIt->second;  // 通过迭代器获取值
        LOG_DEBUG_DELAY("=== CoAP 协议配置 ===");
        for (const auto& item : coapItems) {
            LOG_DEBUG_FMT(":%hd => %s", item.port, item.protocol.c_str());
        }
    }

    // 2. 打印WebSocket大类下的配置（同理改用find()）
    auto wsIt = portProtocolConfig.protocolGroups.find("WebSocket");
    if (wsIt != portProtocolConfig.protocolGroups.end()) {
        const auto& wsItems = wsIt->second;
        LOG_DEBUG_DELAY("=== WebSocket 协议配置 ===");
        for (const auto& item : wsItems) {
            LOG_DEBUG_FMT(":%hd => %s", item.port, item.protocol.c_str());
        }
    }

    // 3. 遍历所有通信大类（C++14兼容，无结构化绑定）
    LOG_DEBUG_DELAY("=== 所有通信大类配置 ===");
    for (const auto& group_pair : portProtocolConfig.protocolGroups) {
        const std::string& groupName = group_pair.first;
        const std::vector<PortProtocolItem>& items = group_pair.second;
        LOG_DEBUG_FMT("大类：%s，子协议数量：%zu", groupName.c_str(), items.size());
    }
}