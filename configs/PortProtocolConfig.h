#pragma once
#ifndef PORT_PROTOCOL_CONFIG_H
#define PORT_PROTOCOL_CONFIG_H

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
    // 单例+禁拷贝+禁移动
    static PortProtocolConfig& getInstance();
    PortProtocolConfig(const PortProtocolConfig&) = delete;
    PortProtocolConfig& operator=(const PortProtocolConfig&) = delete;

    // 解析Config.json，返回「端口→协议大类名称」的映射（如8234→"CoAP"）
    // std::unordered_map<short, std::string> getPortToCategoryMap();
    // 返回值改为三级映射：端口 → 协议大类名称 → 子协议名称
    std::unordered_map<short, std::unordered_map<std::string, std::vector<std::string>>> getPortToCategoryMap();

    // 解析Config.json，返回「协议大类→子协议+端口列表」的映射
    std::unordered_map<ProtocolCategory, std::vector<PortProtocolItem>> getCategoryToItemsMap();

    // 获取「端口→CoAP子协议」映射（适配IotSession的需求）
    std::unordered_map<short, std::vector<std::string>> getCoapSubProtoPortMap();
    // 简化接口：仅获取端口→协议大类的映射（兼容原有逻辑）
    std::unordered_map<short, std::string> getPortToSimpleCategoryMap();

    // // 协议大类名称转枚举（如"CoAP"→ProtocolCategory::CoAP）
    // ProtocolCategory categoryNameToEnum(const std::string& category_name);
    // // 协议大类枚举转名称（如ProtocolCategory::WebSocket→"WebSocket"）
    // std::string categoryEnumToName(ProtocolCategory category);

private:
    PortProtocolConfig() = default;
    ~PortProtocolConfig() = default;

    void parsePortProtocolConfig();  // 合并所有遍历逻辑，填充三级映射
    // 协议大类名称转枚举（如"CoAP"→ProtocolCategory::CoAP）
    ProtocolCategory categoryNameToEnum(const std::string& category_name);
    // 协议大类枚举转名称（如ProtocolCategory::WebSocket→"WebSocket"）
    std::string categoryEnumToName(ProtocolCategory category);

    // 示例：8234 → {"CoAP": "CoAP-RTU"}, 9090 → {"WebSocket": "WebSocket-Web"}
    std::unordered_map<short, std::unordered_map<std::string, std::vector<std::string>>> _port_to_category_to_items;

    // 缓存解析结果，避免重复IO
    // std::unordered_map<short, std::string> _port_to_category;                                // 端口→协议大类名称
    // std::unordered_map<ProtocolCategory, std::vector<PortProtocolItem>> _category_to_items;  // 大类→子项列表
    bool _is_parsed = false;  // 标记是否已解析配置
};

#endif  // PORT_PROTOCOL_CONFIG_H