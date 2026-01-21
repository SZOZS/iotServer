#pragma once
#ifndef FRAME_SD_RTU_PARSER_H
#define FRAME_SD_RTU_PARSER_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../../configs/frames/sd/RTUConfig.h"
#include "../../FrameParserFactory.h"
#include "../../IFrameParser.h"

// RTU协议帧结构
struct FrameRTU : public FrameBase
{
    uint8_t header;            // 帧头（1字节）
    uint8_t serialNumber;      // 业务流水号（1字节）
    uint16_t targetAddress;    // 目标地址（2字节）
    uint16_t sourceAddress;    // 源地址（2字节）
    uint16_t index;            // 索引（2字节）
    uint8_t fixedPadding[7];   // 固定填充（7字节）
    std::string deviceNumber;  // 12位设备编号（12字节）
    uint16_t dataLength;       // 数据长度（2字节）
    uint8_t checksum;          // 校验位（1字节）
    uint8_t tail;              // 帧尾（1字节）
};
// 注册帧
struct FrameRTURegister : public FrameRTU
{
    // 添加构造函数：用基类对象初始化基类部分
    FrameRTURegister(const FrameRTU& base) : FrameRTU(base) {}
    struct
    {
        uint8_t yearHigh;  // 高年（1字节）
        uint8_t yearLow;   // 低年（1字节）
        uint8_t month;     // 月（1字节）
        uint8_t day;       // 日（1字节）
        uint8_t hour;      // 时（1字节）
        uint8_t minute;    // 分（1字节）
        uint8_t second;    // 秒（1字节）
    } rtuTime;             // RTU时间（7字节）
};
// 数据帧
struct FrameRTUReportData : public FrameRTU
{
    // 添加构造函数：用基类对象初始化基类部分
    FrameRTUReportData(const FrameRTU& base) : FrameRTU(base) {}
    uint8_t fixedPadding2[5];  // 预留区固定填充（5字节，大端）
    struct
    {
        uint8_t kValue1;       // 开关量1~8（1字节）
        uint8_t kValue2;       // 开关量9~16（1字节）
        uint8_t kValue3;       // 开关量17~24（1字节）
    } kValue;                  // 开关量（3字节，大端）最大可表示24个开关量
    uint8_t fixedPadding3[2];  // 预留区（2字节，大端）
    struct
    {
        uint16_t m1Value;   // 1号模拟量值（2字节）
        uint16_t m2Value;   // 2号模拟量值（2字节）
        uint16_t m3Value;   // 3号模拟量值（2字节）
        uint16_t m4Value;   // 4号模拟量值（2字节）
        uint16_t m5Value;   // 5号模拟量值（2字节）
        uint16_t m6Value;   // 6号模拟量值（2字节）
        uint16_t m7Value;   // 7号模拟量值（2字节）
        uint16_t m8Value;   // 8号模拟量值（2字节）
        uint16_t m9Value;   // 9号模拟量值（2字节）
        uint16_t m10Value;  // 10号模拟量值（2字节）
        uint16_t m11Value;  // 11号模拟量值（2字节）
        uint16_t m12Value;  // 12号模拟量值（2字节）
        uint16_t m13Value;  // 13号模拟量值（2字节）
        uint16_t m14Value;  // 14号模拟量值（2字节）
        uint16_t m15Value;  // 15号模拟量值（2字节）
        uint16_t m16Value;  // 16号模拟量值（2字节）
    } mValue;               // 模拟量(32字节，大端) 存储规则：真实值放大100倍后的整数，如0.63→63（0x003F），1.6→160（0x00A0）
    uint8_t fixedByte;      // 固定填写1个字节0x0A（1字节，大端）
};

class RTUParser : public IFrameParser
{
public:
    // 构造函数/析构函数
    RTUParser() = default;
    ~RTUParser() = default;

    bool isFrameComplete(const std::vector<uint8_t>& frame) const override
    {
        // 基于帧结构判断完整性：包含完整帧头、帧尾且校验和正确
        if (frame.size() < 2)
            return false;

        // 检查帧头帧尾
        const auto& header = getFrameHeader();
        const auto& tail = getFrameTail();
        if (frame.size() < header.size() + tail.size())
            return false;

        // 检查帧尾
        for (size_t i = 0; i < tail.size(); ++i) {
            if (frame[frame.size() - tail.size() + i] != tail[i]) {
                return false;
            }
        }

        // 最小长度检查
        return frame.size() >= 10;  // 最小帧长度
    }

    bool parse(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg) override;

    bool saveToDB(const FrameBase& frame, const std::string& sessionUuid) override;

    const std::vector<uint8_t>& getFrameHeader() const override;

    const std::vector<uint8_t>& getFrameTail() const override
    {
        static const std::vector<uint8_t> tail = {FRAME_SD_RTU_END};  // 单字节帧尾
        return tail;
    }

    std::vector<uint8_t> getCurrentTimeBCD();

private:
    uint8_t bcdToDecimal(uint8_t bcd);
    bool parseRegisterFrame(const std::vector<uint8_t>& frame, size_t& pos, FrameRTURegister& result);
    bool parseReportDataFrame(const std::vector<uint8_t>& frame, size_t& pos, FrameRTUReportData& result);
};

#endif  // FRAME_SD_RTU_PARSER_H