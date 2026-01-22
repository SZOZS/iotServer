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

// 返回三级映射：端口→协议大类→子协议
std::unordered_map<short, std::unordered_map<std::string, std::vector<std::string>>> PortProtocolConfig::getPortToCategoryMap()
{
    parsePortProtocolConfig();
    return _port_to_category_to_items;
}

// 兼容接口：仅返回端口→协议大类的映射（适配原有调用逻辑）
std::unordered_map<short, std::string> PortProtocolConfig::getPortToSimpleCategoryMap()
{
    parsePortProtocolConfig();
    std::unordered_map<short, std::string> simple_map;

    for (const auto& port_pair : _port_to_category_to_items) {
        short port = port_pair.first;
        const auto& category_map = port_pair.second;
        if (!category_map.empty()) {
            // 取第一个大类（每个端口仅归属一个大类）
            simple_map[port] = category_map.begin()->first;
        }
    }
    return simple_map;
}

void PortProtocolConfig::parsePortProtocolConfig()
{
    if (_is_parsed) {
        return;
    }

    try {
        // 读取配置数据
        const ConfigGlobal::PortProtocolConfigData_t& config_data = ConfigGlobal::getInstance().getConfigPortProtocol();
        LOG_INFO_DELAY("[config][port_protocol] 读取成功");

        // 遍历所有协议大类（正序）
        for (const auto& protocol_group_pair : config_data.protocolGroups) {
            const std::string& category_name = protocol_group_pair.first;
            const std::vector<ConfigGlobal::PortProtocolItem_t>& items = protocol_group_pair.second;
            LOG_DEBUG_FMT_DELAY("[config][port_protocol][%s](子协议数%zu)", category_name.c_str(), items.size());

            // 遍历当前大类下的所有端口+子协议（不覆盖，追加）
            for (const auto& item : items) {
                // 追加子协议到列表，而非覆盖
                _port_to_category_to_items[item.port][category_name].push_back(item.protocol);

                LOG_DEBUG_FMT_DELAY("  └─ [%d] → [%s] → [%s]（%zu）",
                                    item.port,
                                    category_name.c_str(),
                                    item.protocol.c_str(),
                                    _port_to_category_to_items[item.port][category_name].size());
            }
        }

        // 日志输出解析结果
        LOG_INFO_FMT_DELAY("[config][port_protocol] 读取完成，共映射%zu个端口", _port_to_category_to_items.size());
        _is_parsed = true;

    } catch (const std::exception& e) {
        LOG_ERROR_FMT_DELAY("[config][port_protocol] 读取异常【类型：%s】：%s", typeid(e).name(), e.what());
        _is_parsed = true;
    } catch (...) {
        LOG_ERROR_DELAY("[config][port_protocol] 读取发生未知异常！");
        _is_parsed = true;
    }
}

// 返回CoAP大类下的多子协议列表
std::unordered_map<short, std::vector<std::string>> PortProtocolConfig::getCoapSubProtoPortMap()
{
    parsePortProtocolConfig();
    std::unordered_map<short, std::vector<std::string>> coap_sub_proto_map;

    for (const auto& port_pair : _port_to_category_to_items) {
        short port = port_pair.first;
        const auto& category_item = port_pair.second.find("CoAP");

        // 仅提取CoAP大类的子协议
        if (category_item != port_pair.second.end()) {
            coap_sub_proto_map[port] = category_item->second;
        }
    }

    LOG_INFO_FMT_DELAY("提取CoAP子协议映射完成，共%zu个端口", coap_sub_proto_map.size());
    return coap_sub_proto_map;
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
