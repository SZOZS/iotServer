#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "../../../configs/ProtocolConfig.h"
#include "../../../configs/frames/sd/RTUConfig.h"
#include "../../../handlers/mysql/MySQLConnectionPool.h"
#include "../../../logs/Logger.h"
#include "../../../utils/UtilsHex.h"

#include "RTUParser.h"

bool RTUParser::parse(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg)
{
    try {
        size_t pos = 0;
        std::unique_ptr<FrameRTU> result;

        // 解析公共字段
        auto baseFrame = std::make_unique<FrameRTU>();
        baseFrame->protocol = "SD_RTU";
        baseFrame->rawData = frame;

        // 帧头（已通过完整性检查）
        baseFrame->header = frame[pos++];

        // 业务流水号
        baseFrame->serialNumber = frame[pos++];

        // 目标地址
        baseFrame->targetAddress = (static_cast<uint16_t>(frame[pos]) << 8) | frame[pos + 1];
        pos += 2;

        // 源地址
        baseFrame->sourceAddress = (static_cast<uint16_t>(frame[pos]) << 8) | frame[pos + 1];
        pos += 2;

        // 索引
        baseFrame->index = (static_cast<uint16_t>(frame[pos]) << 8) + frame[pos + 1];
        pos += 2;

        // 固定填充
        for (int i = 0; i < 7; i++) {
            baseFrame->fixedPadding[i] = frame[pos++];
        }

        // 设备编号
        for (int i = 0; i < 12; i++) {
            baseFrame->deviceNumber += static_cast<char>(frame[pos++]);
        }

        // 数据长度
        baseFrame->dataLength = (static_cast<uint16_t>(frame[pos]) << 8) | frame[pos + 1];
        pos += 2;

        // 根据数据长度判断帧类型并解析
        if (baseFrame->dataLength == 7) {  // 注册帧数据长度
            auto registerFrame = std::make_unique<FrameRTURegister>(*baseFrame);
            registerFrame->msgId = MSG_SD_RTU_REGISTER;  // 分配消息ID
            if (!parseRegisterFrame(frame, pos, *registerFrame)) {
                errorMsg = "注册帧解析失败";
                return false;
            }
            outFrame = std::move(registerFrame);
        } else if (baseFrame->dataLength == 43) {  // 数据帧数据长度
            auto dataFrame = std::make_unique<FrameRTUReportData>(*baseFrame);
            dataFrame->msgId = MSG_SD_RTU_REPORT_DATA;  // 分配消息ID
            if (!parseReportDataFrame(frame, pos, *dataFrame)) {
                errorMsg = "数据帧解析失败";
                return false;
            }
            outFrame = std::move(dataFrame);
        } else {
            errorMsg = "未知数据长度: " + std::to_string(baseFrame->dataLength);
            return false;
        }

        // 校验和
        auto& sdFrame = static_cast<FrameRTU&>(*outFrame);
        sdFrame.checksum = frame[pos++];

        // 校验和验证
        uint8_t calculatedChecksum = 0;
        for (size_t i = 1; i < frame.size() - 2; ++i) {
            calculatedChecksum += frame[i];
        }
        calculatedChecksum &= 0xFF;
        if (calculatedChecksum != sdFrame.checksum) {
            errorMsg = "校验和不匹配";
            return false;
        }

        // 帧尾（已通过完整性检查）
        sdFrame.tail = frame[pos++];

        return true;
    } catch (const std::exception& e) {
        errorMsg = "解析异常: " + std::string(e.what());
        return false;
    }
}

bool RTUParser::parseRegisterFrame(const std::vector<uint8_t>& frame, size_t& pos, FrameRTURegister& result)
{
    result.rtuTime.yearHigh = frame[pos++];
    result.rtuTime.yearLow = frame[pos++];
    result.rtuTime.month = frame[pos++];
    result.rtuTime.day = frame[pos++];
    result.rtuTime.hour = frame[pos++];
    result.rtuTime.minute = frame[pos++];
    result.rtuTime.second = frame[pos++];
    return true;
}

bool RTUParser::parseReportDataFrame(const std::vector<uint8_t>& frame, size_t& pos, FrameRTUReportData& result)
{
    // 解析固定填充
    for (int i = 0; i < 5; i++) {
        result.fixedPadding2[i] = frame[pos++];
    }

    // 开关量
    result.kValue.kValue1 = frame[pos++];
    result.kValue.kValue2 = frame[pos++];
    result.kValue.kValue3 = frame[pos++];

    // 固定填充
    for (int i = 0; i < 2; i++) {
        result.fixedPadding3[i] = frame[pos++];
    }

    // 模拟量
    uint16_t* mValues[] = {&result.mValue.m1Value,
                           &result.mValue.m2Value,
                           &result.mValue.m3Value,
                           &result.mValue.m4Value,
                           &result.mValue.m5Value,
                           &result.mValue.m6Value,
                           &result.mValue.m7Value,
                           &result.mValue.m8Value,
                           &result.mValue.m9Value,
                           &result.mValue.m10Value,
                           &result.mValue.m11Value,
                           &result.mValue.m12Value,
                           &result.mValue.m13Value,
                           &result.mValue.m14Value,
                           &result.mValue.m15Value,
                           &result.mValue.m16Value};
    for (auto val : mValues) {
        *val = (static_cast<uint16_t>(frame[pos]) << 8) | frame[pos + 1];
        pos += 2;
    }

    result.fixedByte = frame[pos++];
    return result.fixedByte == 0x0A;
}

bool RTUParser::saveToDB(const FrameBase& frame, const std::string& sessionUuid)
{
    const auto& sdFrame = static_cast<const FrameRTU&>(frame);
    auto mysqlPool = MySQLConnectionPool::ConnectionGuard(MySQLConnectionPool::getInstance().getConnection());
    if (!mysqlPool.get()) {
        LOG_ERROR("获取MySQL连接失败");
        return false;
    }

    std::map<std::string, std::string> frameData;
    frameData["device_number"] = sdFrame.deviceNumber;
    frameData["frame_raw"] = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(sdFrame.rawData.data()), sdFrame.rawData.size());
    frameData["protocal_type"] = "SD_RTU";
    frameData["frame_type"] = sdFrame.msgId == MSG_SD_RTU_REGISTER ? "register" : "report_data";
    frameData["data_length"] = std::to_string(sdFrame.dataLength);
    frameData["session_uuid"] = sessionUuid;
    frameData["serial_number"] = std::to_string(sdFrame.serialNumber);
    frameData["source_address"] = std::to_string(sdFrame.sourceAddress);

    return mysqlPool->insertOne("pool_iot_devices_data_frame_raw", frameData);
}

const std::vector<uint8_t>& RTUParser::getFrameHeader() const
{
    static const std::vector<uint8_t> header = []() {
        auto* config = ProtocolConfigManager::getInstance().getConfig("SD_RTU");
        if (!config)
            LOG_ERROR("获取SD_RTU协议配置失败");
        return config->header;
    }();
    return header;
}

uint8_t RTUParser::bcdToDecimal(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 新增：获取当前时间的BCD码
std::vector<uint8_t> RTUParser::getCurrentTimeBCD()
{
    std::vector<uint8_t> timeBCD(7);  // 7字节：年高、年低、月、日、时、分、秒

    // 获取当前系统时间
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_r(&time, &localTime);  // 转换为本地时间

    // 转换为BCD码（注意：BCD码每个十进制数字用4位二进制表示）
    int year = localTime.tm_year + 1900;  // tm_year是从1900年开始的偏移量
    timeBCD[0] = (year / 100) % 100;      // 年高位（如2024年 → 20 → 0x20）
    timeBCD[1] = year % 100;              // 年低位（如2024年 → 24 → 0x24）
    timeBCD[2] = localTime.tm_mon + 1;    // 月（tm_mon从0开始，+1）
    timeBCD[3] = localTime.tm_mday;       // 日
    timeBCD[4] = localTime.tm_hour;       // 时
    timeBCD[5] = localTime.tm_min;        // 分
    timeBCD[6] = localTime.tm_sec;        // 秒

    // 将每个值转换为BCD码（如 12 → 0x12）
    for (auto& val : timeBCD) {
        val = ((val / 10) << 4) | (val % 10);
    }

    return timeBCD;
}
