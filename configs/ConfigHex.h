#pragma once
#ifndef CONFIG_HEX_H
#define CONFIG_HEX_H

#include "ConfigGlobal.h"

// 十六进制转换配置类（单例模式）
class ConfigHex
{
private:
    // 私有构造函数：初始化，默认配置
    ConfigHex()
    {
        auto& globalHex = ConfigGlobal::getInstance().getConfigHex();
        m_hexUppercase = globalHex.hexUppercase;
        m_hexWithSpace = globalHex.hexWithSpace;
    }

    // 禁止拷贝和赋值
    ConfigHex(const ConfigHex&) = delete;
    ConfigHex& operator=(const ConfigHex&) = delete;

    bool m_hexUppercase;  // 十六进制转换，是否使用大写字母
    bool m_hexWithSpace;  // 十六进制转换，是否在字节间添加空格

public:
    // 获取单例实力
    static ConfigHex& getInstance()
    {
        static ConfigHex instance;
        return instance;
    }

    // 获取大小写配置
    bool isHexUppercase() const { return m_hexUppercase; }
    // 修改大小写配置（如需动态调整）
    void setHexUppercase(bool uppercase) { m_hexUppercase = uppercase; }

    // 获取空格分割配置
    bool isWithSpace() const { return m_hexWithSpace; }
    void setWithSpace(bool withSpace) { m_hexWithSpace = withSpace; }
};

#endif  // CONFIG_HEX_H