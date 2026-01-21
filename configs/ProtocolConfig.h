#pragma once
#ifndef PROTOCOL_CONFIG_H
#define PROTOCOL_CONFIG_H

#include <string>
#include <unordered_map>
#include <vector>

#include "../patterns/Singleton.h"  // 引入单例基类

struct ProtocolConfig
{
    std::string name;             // 协议名（如"SD_RTU"）
    std::vector<uint8_t> header;  // 帧头
    std::vector<uint8_t> tail;    // 帧尾
    uint32_t maxFrameLength;      // 最大帧长
    std::string checksumType;     // 校验方式（如"CRC16"、"XOR"）
};

// 继承 Singleton 实现单例，统一类定义
class ProtocolConfigManager : public Singleton<ProtocolConfigManager>
{
    friend class Singleton<ProtocolConfigManager>;  // 允许 Singleton 访问私有构造函数
private:
    ProtocolConfigManager() = default;  // 私有构造函数，确保单例
    std::unordered_map<std::string, ProtocolConfig> m_configs;

public:
    // 从配置文件加载（如JSON）
    // bool loadFromFile(const std::string& path);
    // 新增：从代码加载配置（之前的 loadConfigs 功能）
    void loadConfigs();
    // 获取协议配置
    const ProtocolConfig* getConfig(const std::string& protocol) const;
};

#endif  // PROTOCOL_CONFIG_H