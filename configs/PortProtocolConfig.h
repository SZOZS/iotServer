#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// 子协议+端口结构体（匹配Config.json的port_protocol子项）
struct ProtocolPortItem
{
    std::string sub_protocol;  // 子协议（如SD_RTU/WEB_SOCKET）
    short port;                // 端口号（对应Config.json的ports字段）
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
    std::unordered_map<ProtocolCategory, std::vector<ProtocolPortItem>> getCategoryToItemsMap();

    // 辅助方法：协议大类名称转枚举（如"CoAP"→ProtocolCategory::CoAP）
    ProtocolCategory categoryNameToEnum(const std::string& category_name);

    // 辅助方法：协议大类枚举转名称（如ProtocolCategory::WebSocket→"WebSocket"）
    std::string categoryEnumToName(ProtocolCategory category);

private:
    // 私有构造：仅单例可创建
    PortProtocolConfig();
    // 禁止拷贝
    PortProtocolConfig(const PortProtocolConfig&) = delete;
    PortProtocolConfig& operator=(const PortProtocolConfig&) = delete;

    // 解析Config.json的port_protocol节点（内部核心逻辑）
    void parsePortProtocolConfig();

    // 缓存解析结果，避免重复IO
    std::unordered_map<short, std::string> _port_to_category;                                // 端口→协议大类名称
    std::unordered_map<ProtocolCategory, std::vector<ProtocolPortItem>> _category_to_items;  // 大类→子项列表
    bool _is_parsed = false;                                                                 // 标记是否已解析配置
};