#pragma once
#ifndef CONFIG_GLOBAL_H
#define CONFIG_GLOBAL_H

#include <climits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../thrid_partys/json.hpp"

using json = nlohmann::json;

class ConfigGlobal
{
private:
    ConfigGlobal() { loadConfig("configs/Config.json"); }
    ~ConfigGlobal() = default;

    ConfigGlobal(const ConfigGlobal&) = delete;
    ConfigGlobal& operator=(const ConfigGlobal) = delete;

    void loadConfig(const std::string& filePath);

    struct HexConfigData
    {
        bool hexUppercase = true;
        bool hexWithSpace = true;
    } m_configHex;

    struct LogConfigData
    {
        std::string filePath = "app.log";
    } m_configLog;

    struct MySQLConfigData
    {
        std::string host;
        std::string user;
        std::string password;
        std::string database;
        unsigned int port;
        unsigned int timeout;
        bool softDelete;
        unsigned int maxRetries;
        unsigned int retryDelay;
        unsigned int maxConnections;
        unsigned int connectionMaxLifetime;
        unsigned int maxIdleTime;
    } m_configMySQL;

    struct RedisConfigData
    {
        std::string host;
        unsigned int port;
        std::string password;
        int db;
        unsigned int timeout;
        unsigned int maxRetries;
        unsigned int retryDelay;
        unsigned int maxConnections;
        unsigned int connectionMaxLifetime;
        unsigned int maxIdleTime;
    } m_configRedis;

    struct PhpPushConfigData
    {
        std::unordered_set<std::string> ip_whitelist;  // 存储白名单IP
    } m_configPhpPush;

    // 单个端口协议项（如SD_RTU/8234）
    struct PortProtocolItem
    {
        std::string protocol;  // 子协议名（如SD_RTU/SD_YC/...）
        short port;            // 端口号（对应JSON中的ports字段）
    };

    // 协议大类配置：匹配JSON中的"port_protocol"根节点
    struct PortProtocolConfigData
    {
        // ✅ key：协议大类（MQTT/CoAP/WebSocket等），匹配JSON的一级键
        // ✅ value：子协议列表，匹配JSON中CoAP/WebSocket下的数组
        std::unordered_map<std::string, std::vector<PortProtocolItem>> protocolGroups;
    };

    PortProtocolConfigData m_configPortProtocol;

public:
    static ConfigGlobal& getInstance()
    {
        static ConfigGlobal instance;
        return instance;
    }

    const HexConfigData& getConfigHex() const { return m_configHex; }
    const LogConfigData& getConfigLog() const { return m_configLog; }
    const MySQLConfigData& getConfigMySQL() const { return m_configMySQL; }
    const RedisConfigData& getConfigRedis() const { return m_configRedis; }
    const PhpPushConfigData& getConfigPhpPush() const { return m_configPhpPush; }

    static std::vector<short> getUniquePortsFromConfig();
    std::string getUniquePortsFromConfig(short port);

    static void logPortProtocolConfig();

    // ========== 新增：公有类型别名（让外部能引用私有结构体） ==========
    using PortProtocolItem_t = PortProtocolItem;
    using PortProtocolConfigData_t = PortProtocolConfigData;

    // ========== 新增：公有访问接口（获取私有配置实例） ==========
    const PortProtocolConfigData_t& getConfigPortProtocol() const { return m_configPortProtocol; }

    // 获取日志配置
};

#endif  // CONFIG_GLOBAL_H