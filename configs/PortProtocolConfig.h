#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// 1. 声明PortProtocolItem结构体（解决"未声明标识符"错误）
struct PortProtocolItem
{
    short port;            // 端口号
    std::string protocol;  // 协议类型（CoAP/WebSocket/MQTT）
    // 可补充其他字段（如超时时间、最大连接数等）
};

// 协议大类分组（匹配Config.json的port_protocol顶层结构）
enum class ProtocolCategory
{
    UNKNOWN,
    MQTT,
    CoAP,
    HTTP_HTTPS,
    WebSocket,
    MQTT_SN,
    LwM2M,
    AMQP
};

// 端口-协议配置核心类（单例模式）
class PortProtocolConfig
{
public:
    // 获取单例实例
    static PortProtocolConfig& getInstance();

    // 核心方法：解析Config.json，返回「端口→协议大类名称」的映射（如8234→"CoAP"）
    std::unordered_map<short, std::string> getPortToCategoryMap();

    // 辅助方法：解析Config.json，返回「协议大类→子协议+端口列表」的映射
    std::unordered_map<ProtocolCategory, std::vector<PortProtocolItem>> getCategoryToItemsMap();

    // 辅助方法：协议大类名称转枚举（如"CoAP"→ProtocolCategory::CoAP）
    ProtocolCategory categoryNameToEnum(const std::string& category_name);

    // 辅助方法：协议大类枚举转名称（如ProtocolCategory::WebSocket→"WebSocket"）
    std::string categoryEnumToName(ProtocolCategory category);

    // 禁用拷贝（单例必备）
    PortProtocolConfig(const PortProtocolConfig&) = delete;
    PortProtocolConfig& operator=(const PortProtocolConfig&) = delete;

private:
    PortProtocolConfig() = default;  // 私有构造
    void parsePortProtocolConfig();  // 核心解析方法
    // 禁止拷贝
    PortProtocolConfig(const PortProtocolConfig&) = delete;
    PortProtocolConfig& operator=(const PortProtocolConfig&) = delete;

    // 缓存解析结果，避免重复IO
    std::unordered_map<short, std::string> _port_to_category;                                // 端口→协议大类名称
    std::unordered_map<ProtocolCategory, std::vector<PortProtocolItem>> _category_to_items;  // 大类→子项列表
    bool _is_parsed = false;                                                                 // 标记是否已解析配置
};