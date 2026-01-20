#pragma once
#ifndef CONFIG_GLOBAL_H
#define CONFIG_GLOBAL_H

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

    struct PortProtocolConfigData
    {
        // key：通信大类（MQTT/CoAP/HTTP/HTTPS/WebSocket等），value：对应子协议列表
        std::unordered_map<std::string, std::vector<PortProtocolItem>> protocolGroups;
    } m_configPortProtocol;  // 端口协议配置实例

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
    const PortProtocolConfigData& getConfigPortProtocol() const { return m_configPortProtocol; }

    static std::vector<short> getUniquePortsFromConfig();
    static void logPortProtocolConfig();
    // 获取日志配置
};

#endif  // CONFIG_GLOBAL_H