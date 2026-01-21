#include <cstring>

#include "../../../utils/UtilsHex.h"

#include "YCBuilder.h"

std::vector<uint8_t> YCBuilder::buildConfirmFrame(const FrameGB& requestFrame)
{
    FrameConfirm confirmFrame(requestFrame);
    return buildFrame(confirmFrame);
}

std::vector<uint8_t> YCBuilder::buildDenyFrame(const FrameGB& requestFrame)
{
    FrameDeny denyFrame(requestFrame);
    return buildFrame(denyFrame);
}

// 通用帧构建逻辑（提取为私有方法）
std::vector<uint8_t> YCBuilder::buildFrame(const FrameGB& frame)
{
    std::vector<uint8_t> result;

    // 1. 帧头（2字节）
    result.push_back(frame.header[0]);
    result.push_back(frame.header[1]);

    // 2. 业务流水号（2字节，低字节在前）
    result.push_back(static_cast<uint8_t>(frame.serialNumber & 0xFF));
    result.push_back(static_cast<uint8_t>((frame.serialNumber >> 8) & 0xFF));

    // 3. 协议版本号（2字节）
    result.push_back(frame.version[0]);
    result.push_back(frame.version[1]);

    // 4. 时间标签（6字节）
    for (const auto& byte : frame.timeTag) {
        result.push_back(byte);
    }

    // 5. 源地址与目的地址交换（反馈帧需原路返回）
    for (const auto& byte : frame.targetAddress) {  // 原目的地址变为新源地址
        result.push_back(byte);
    }
    for (const auto& byte : frame.sourceAddress) {  // 原源地址变为新目的地址
        result.push_back(byte);
    }

    // 6. 应用数据单元长度（2字节，低字节在前，确认/否认帧为0）
    result.push_back(static_cast<uint8_t>(frame.dataLength & 0xFF));
    result.push_back(static_cast<uint8_t>((frame.dataLength >> 8) & 0xFF));

    // 7. 命令字节（确认0x03/否认0x06）
    result.push_back(frame.commandByte);

    // 8. 校验和（预留位置，后续计算后替换）
    result.push_back(0x00);

    // 9. 帧尾（2字节）
    result.push_back(frame.tail[0]);
    result.push_back(frame.tail[1]);

    // 计算并填充校验和（范围：帧头后第1字节到数据部分结束）
    uint8_t checksum = 0;
    for (size_t i = 2; i < result.size() - 3; ++i) {  // 跳过帧头2字节，排除校验和及帧尾
        checksum += result[i];
    }
    checksum &= 0xFF;
    result[result.size() - 3] = checksum;  // 替换预留的校验和位置

    return result;
}

uint8_t YCBuilder::calculateChecksum(const std::vector<uint8_t>& frameData)
{
    uint8_t checksum = 0;
    for (const auto& byte : frameData) {
        checksum += byte;
    }
    return checksum & 0xFF;
}