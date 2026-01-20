#include "../logs/Logger.h"          // 日志工具
#include "../thrid_partys/json.hpp"  // JSON解析库（项目第三方库）

#include "ConfigGlobal.h"  // 项目原有全局配置类
#include "PortProtocolConfig.h"

using json = nlohmann::json;

// 单例实例初始化
PortProtocolConfig& PortProtocolConfig::getInstance()
{
    static PortProtocolConfig instance;
    return instance;
}

// 构造函数：空实现，解析逻辑延迟到第一次调用时执行
PortProtocolConfig::PortProtocolConfig() {}

// 核心：解析Config.json的port_protocol，生成端口→协议大类映射
void PortProtocolConfig::parsePortProtocolConfig()
{
    if (_is_parsed) {
        return;  // 已解析过，直接返回缓存
    }

    try {
        // 1. 从ConfigGlobal读取PortProtocolConfigData（匹配项目实际结构体）
        // const PortProtocolConfigData& config_data = ConfigGlobal::getInstance().getConfigPortProtocol();
        const ConfigGlobal::PortProtocolConfigData_t& config_data = ConfigGlobal::getInstance().getConfigPortProtocol();

        // 2. 解析CoAP大类（核心：遍历protocolGroups["CoAP"]）
        auto coap_it = config_data.protocolGroups.find("CoAP");
        if (coap_it != config_data.protocolGroups.end()) {
            // 修复PortProtocolItem未声明：用ConfigGlobal::PortProtocolItem_t
            const std::vector<ConfigGlobal::PortProtocolItem_t>& coap_items = coap_it->second;
            for (const auto& item : coap_items) {
                _port_to_category[item.port] = "CoAP";  // 端口→CoAP大类映射
            }
        }

        // 3. 解析WebSocket大类
        auto ws_it = config_data.protocolGroups.find("WebSocket");
        if (ws_it != config_data.protocolGroups.end()) {
            const std::vector<ConfigGlobal::PortProtocolItem_t>& ws_items = ws_it->second;
            for (const auto& item : ws_items) {
                _port_to_category[item.port] = "WebSocket";
            }
        }

        // 4. 扩展：解析MQTT大类（按需添加）
        auto mqtt_it = config_data.protocolGroups.find("MQTT");
        if (mqtt_it != config_data.protocolGroups.end()) {
            const std::vector<ConfigGlobal::PortProtocolItem_t>& mqtt_items = mqtt_it->second;
            for (const auto& item : mqtt_items) {
                _port_to_category[item.port] = "MQTT";
            }
        }

        // 修复日志格式化：size_t用%zu而非%d
        LOG_INFO_FMT("解析port_protocol完成，共映射%zu个端口", _port_to_category.size());
        _is_parsed = true;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT("解析port_protocol异常：%s", e.what());
        _is_parsed = true;  // 标记已解析，避免重复报错
    }
}

// 对外提供：端口→协议大类名称映射
std::unordered_map<short, std::string> PortProtocolConfig::getPortToCategoryMap()
{
    parsePortProtocolConfig();  // 首次调用时解析配置
    return _port_to_category;
}

// 对外提供：协议大类→子协议+端口列表映射（补充完整实现）
std::unordered_map<ProtocolCategory, std::vector<PortProtocolItem>> PortProtocolConfig::getCategoryToItemsMap()
{
    parsePortProtocolConfig();

    // 若需要返回大类→子项列表，可补充如下逻辑（按需启用）
    // if (_category_to_items.empty()) {
    //     // 遍历config_data.protocolGroups，填充_category_to_items
    // }
    return _category_to_items;
}

// 协议大类名称转枚举
ProtocolCategory PortProtocolConfig::categoryNameToEnum(const std::string& category_name)
{
    if (category_name == "CoAP")
        return ProtocolCategory::CoAP;
    if (category_name == "WebSocket")
        return ProtocolCategory::WebSocket;
    if (category_name == "MQTT")
        return ProtocolCategory::MQTT;
    if (category_name == "HTTP/HTTPS")
        return ProtocolCategory::HTTP_HTTPS;
    if (category_name == "MQTT-SN")
        return ProtocolCategory::MQTT_SN;
    if (category_name == "LwM2M")
        return ProtocolCategory::LwM2M;
    if (category_name == "AMQP")
        return ProtocolCategory::AMQP;
    return ProtocolCategory::UNKNOWN;
}

// 协议大类枚举转名称
std::string PortProtocolConfig::categoryEnumToName(ProtocolCategory category)
{
    switch (category) {
        case ProtocolCategory::CoAP:
            return "CoAP";
        case ProtocolCategory::WebSocket:
            return "WebSocket";
        case ProtocolCategory::MQTT:
            return "MQTT";
        case ProtocolCategory::HTTP_HTTPS:
            return "HTTP/HTTPS";
        case ProtocolCategory::MQTT_SN:
            return "MQTT-SN";
        case ProtocolCategory::LwM2M:
            return "LwM2M";
        case ProtocolCategory::AMQP:
            return "AMQP";
        default:
            return "UNKNOWN";
    }
}