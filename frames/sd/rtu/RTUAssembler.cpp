#include "../../../configs/frames/sd/RTUConfig.h"
#include "../../../logs/Logger.h"

#include "RTUAssembler.h"

void RTUAssembler::reset()
{
    frameBuffer_.clear();
    isHeaderFound_ = false;
    isFrameComplete_ = false;
    isDataLengthParsed_ = false;
    dataLength_ = 0;
    remainingDataToReceive_ = 0;
    isDataReceived_ = false;
    isChecksumReceived_ = false;
    checksum_ = 0;
}

bool RTUAssembler::processByte(uint8_t byte)
{
    if (isFrameComplete_)
        reset();  // 处理完一帧后重置，准备下一帧

    // 1. 寻找帧头
    if (!isHeaderFound_) {
        if (byte != FRAME_SD_RTU_START)
            return false;  // 未找到帧头，忽略当前字节
        isHeaderFound_ = true;
        LOG_DEBUG_FMT("找到帧头 0x%02X", byte);
        frameBuffer_.push_back(byte);  // 存入帧头（index0）
        return false;
    }

    // 2. 已找到帧头，收集字节并解析后续数据长度（27、28字节，对应index 28、29）
    frameBuffer_.push_back(byte);
    // LOG_DEBUG_FMT("0x%02X 当前缓冲区长度: %zu", byte, frameBuffer_.size());
    // 2.1 解析27、28字节（需缓冲区包含index 0-28共29字节）
    if (!isDataLengthParsed_ && frameBuffer_.size() >= 29) {
        // 解析数据长度（大端模式：27字节为高8位，28字节为低8位）
        dataLength_ = (static_cast<uint16_t>(frameBuffer_[27]) << 8) | frameBuffer_[28];
        LOG_DEBUG_FMT("0x%02X 0x%02X 解析出后续数据长度: %d 字节", frameBuffer_[27], frameBuffer_[28], dataLength_);
        // 初始化后续数据接收计数器
        remainingDataToReceive_ = dataLength_;
        isDataLengthParsed_ = true;
        return false;
    }

    // 3. 接收后续数据（长度为dataLength_）
    if (isDataLengthParsed_ && !isDataReceived_) {
        // LOG_DEBUG_FMT("接收后续数据，剩余需接收: %zu", remainingDataToReceive_);
        remainingDataToReceive_--;
        // 当剩余接收数为0时，标记数据部分接收完成
        if (remainingDataToReceive_ == 0) {
            isDataReceived_ = true;
            LOG_DEBUG_FMT("后续数据接收完成，共接收 %d 字节", dataLength_);
        }
        return false;
    }

    // 4. 接收校验和（1字节）
    if (isDataReceived_ && !isChecksumReceived_) {
        LOG_DEBUG_FMT("接收校验和，值: 0x%02X", byte);
        // 计算校验和（从帧头后index 1到数据部分结束，即校验和前）
        size_t currentBufferSize = frameBuffer_.size();
        uint8_t calculatedChecksum = 0;
        // LOG_DEBUG_FMT("开始计算校验和，参与字节范围：索引1 至 索引%zu（共%zu字节）", currentBufferSize - 2, (currentBufferSize - 1) - 1); //
        // 计算参与的总字节数 （因为当前字节是校验和，在缓冲区中的index为currentBufferSize - 1）
        for (size_t i = 1; i < currentBufferSize - 1; ++i) {
            calculatedChecksum += frameBuffer_[i];
            // 打印索引和字节值（十六进制）
            // LOG_DEBUG_FMT("参与校验和计算 - 索引: %zu, 字节值: 0x%02X", i, frameBuffer_[i]);
        }
        calculatedChecksum &= 0xFF;  // 取低8位
        // 校验和验证
        if (calculatedChecksum != byte) {
            LOG_ERROR_FMT("校验和不匹配，计算值=0x%02X，帧中值=0x%02X", calculatedChecksum, byte);
            reset();
            return false;
        }
        // 校验通过，标记校验和接收完成
        isChecksumReceived_ = true;
        LOG_DEBUG("校验和验证通过");
        return false;
    }

    // 5. 校验帧尾（0x7D）
    if (isChecksumReceived_) {
        LOG_DEBUG_FMT("校验帧尾，当前字节: 0x%02X", byte);
        if (byte != FRAME_SD_RTU_END) {
            LOG_ERROR_FMT("帧尾错误，预期0x%02X，实际0x%02X", FRAME_SD_RTU_END, byte);
            reset();
            return false;
        }
        // 所有校验通过，标记帧完成
        isFrameComplete_ = true;
        LOG_DEBUG("接收帧完成");
        return true;
    }

    // 6. 防止缓冲区溢出（超过最大帧长则重置，参考原逻辑）
    if (frameBuffer_.size() > FRAME_RTU_MAX_LEN) {
        LOG_WARNING_FMT("RTU帧超过最大长度%d，重置组装器", FRAME_RTU_MAX_LEN);
        reset();
    }

    return false;
}