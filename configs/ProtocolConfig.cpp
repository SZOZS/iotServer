#include "ProtocolConfig.h"
#include "frames/hk/WGConfig.h"    // 包含 HK_WG 协议的配置常量
#include "frames/hy/LoRaConfig.h"  // 包含 HY_LORA 协议的配置常量
#include "frames/sd/RTUConfig.h"   // 包含 SD_RTU 协议的配置常量
#include "frames/sd/YCConfig.h"    // 包含 SD_YC 协议的配置常量

void ProtocolConfigManager::loadConfigs()
{
    // 加载 SD_RTU 协议配置
    ProtocolConfig rtuConfig;
    rtuConfig.name = "SD_RTU";
    rtuConfig.header = {FRAME_SD_RTU_START};       // 从 RTUConfig.h获取帧头定义
    rtuConfig.tail = {FRAME_SD_RTU_END};           // 从 RTUConfig.h获取帧尾定义
    rtuConfig.maxFrameLength = FRAME_RTU_MAX_LEN;  // 最大帧长
    rtuConfig.checksumType = "XOR";                // 校验方式
    m_configs["SD_RTU"] = rtuConfig;

    // 加载 HY_LORA 协议配置
    ProtocolConfig loraConfig;
    loraConfig.name = "HY_LORA";
    loraConfig.header = {FRAME_HY_LORA_START};       // 从 LoRaConfig.h获取帧头定义
    loraConfig.tail = {FRAME_HY_LORA_END};           // 从 LoRaConfig.h获取帧尾定义
    loraConfig.maxFrameLength = FRAME_LORA_MAX_LEN;  // 最大帧长
    loraConfig.checksumType = "XOR";                 // 校验方式
    m_configs["HY_LORA"] = loraConfig;

    // 加载 HY_LORA 协议配置
    ProtocolConfig ycConfig;
    ycConfig.name = "SD_YC";
    ycConfig.header = {GB_FRAME_HEADER};         // 从 YCConfig.h获取帧头定义
    ycConfig.tail = {GB_FRAME_TAIL};             // 从 YCConfig.h获取帧尾定义
    ycConfig.maxFrameLength = GB_MAX_FRAME_LEN;  // 最大帧长
    ycConfig.checksumType = "XOR";               // 校验方式
    m_configs["SD_YC"] = ycConfig;

    // 加载 HK_WG 协议配置
    ProtocolConfig hkwgConfig;
    hkwgConfig.name = "HK_WG";
    hkwgConfig.header = {HK_WG_FRAME_HEADER};         // 从 hkwgConfig.h获取帧头定义
    hkwgConfig.tail = {HK_WG_FRAME_TAIL};             // 从 hkwgConfig.h获取帧尾定义
    hkwgConfig.maxFrameLength = HK_WG_MAX_FRAME_LEN;  // 最大帧长
    hkwgConfig.checksumType = "XOR";                  // 校验方式
    m_configs["HK_WG"] = hkwgConfig;

    // 可添加其他协议配置...
}

// bool ProtocolConfigManager::loadFromFile(const std::string& path) {
//   // 如需从文件加载配置，实现此方法（当前可留空）
//   return true;
// }

const ProtocolConfig* ProtocolConfigManager::getConfig(const std::string& protocol) const
{
    auto it = m_configs.find(protocol);
    return (it != m_configs.end()) ? &(it->second) : nullptr;
}