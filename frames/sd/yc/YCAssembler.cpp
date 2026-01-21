#include "../../../logs/Logger.h"
#include "../../../utils/UtilsHex.h"

#include "YCAssembler.h"

void YCAssembler::reset()
{
    frameBuffer_.clear();
    isHeaderFound_ = false;
    isDataLengthParsed_ = false;
    dataLength_ = 0;
    remainingDataToReceive_ = 0;
    isCommandByteValidated_ = false;
    receivedDataCount_ = 0;
    isChecksumReceived_ = false;
    isFrameComplete_ = false;
    headerMatchCount_ = 0;
}

bool YCAssembler::processByte(uint8_t byte)
{
    if (isFrameComplete_)
        reset();

    // 1·寻找帧头
    if (!isHeaderFound_) {
        static const std::vector<uint8_t> header = GB_FRAME_HEADER;  // 多字节帧头
        if (byte == header[headerMatchCount_]) {
            // 匹配当前字节，计数+1
            headerMatchCount_++;
            frameBuffer_.push_back(byte);
            // 若匹配完所有帧头字节，则标记找到帧头并打印完整帧头
            if (headerMatchCount_ == header.size()) {
                isHeaderFound_ = true;
                // 构建完整帧头的十六进制字符串（如 "0x68, 0x01, 0x02"）
                std::stringstream headerStr;
                for (size_t i = 0; i < header.size(); ++i) {
                    if (i > 0) {
                        headerStr << ", ";  // 非首字节前加逗号分隔
                    }
                    headerStr << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(header[i]);
                }
                // 调用LOG_DEBUG_FMT打印完整帧头
                // LOG_DEBUG_FMT("帧头[%s]已找到，开始组装帧...", headerStr.str().c_str());
            }
        } else {
            // 匹配失败，重置状态
            reset();
        }
        return false;
    }

    // 2. 已找到帧头，收集字节并解析后续数据长度（24、25字节，对应index 25、26）
    if (!isDataLengthParsed_) {
        frameBuffer_.push_back(byte);
        // 当缓冲区长度达到26字节时，解析24-25字节作为数据长度
        if (frameBuffer_.size() >= 26) {
            dataLength_ = frameBuffer_[24] | (static_cast<uint16_t>(frameBuffer_[25]) << 8);
            if (dataLength_ > 1024) {
                LOG_ERROR_FMT("应用数据单元长度超出限制，解析值=%d，最大允许=1024", dataLength_);
                reset();
                return false;
            }
            // LOG_DEBUG_FMT("长度[0x%02X,0x%02X]解析完成: %d 字节（小端模式）", frameBuffer_[24], frameBuffer_[25], dataLength_);
            remainingDataToReceive_ = dataLength_;  // 待接收数据长度
            isDataLengthParsed_ = true;
        }
        return false;
    }

    // 3. 处理第27字节（命令字节）
    if (!isCommandByteValidated_) {
        frameBuffer_.push_back(byte);
        // 第27字节对应索引26（0-based）
        if (frameBuffer_.size() == 27) {
            uint8_t commandByte = frameBuffer_[26];
            // 验证命令字节是否在枚举范围内（1-6）
            if (commandByte < 1 || commandByte > 6) {
                LOG_ERROR_FMT("命令字节无效: 0x%02X，必须为1-6", commandByte);
                reset();
                return false;
            }
            // LOG_DEBUG_FMT("命令字节验证通过: %hhu（%s）", commandByte,
            //               commandByte == 1   ? "控制命令"
            //               : commandByte == 2 ? "发送数据"
            //               : commandByte == 3 ? "确认"
            //               : commandByte == 4 ? "请求"
            //               : commandByte == 5 ? "应答"
            //                                  : "否认");
            isCommandByteValidated_ = true;
        }
        return false;
    }
    // 4. 接收数据部分（从第28字节开始，共dataLength_字节）
    if (receivedDataCount_ < dataLength_) {
        frameBuffer_.push_back(byte);
        receivedDataCount_++;
        // 数据接收完成后，准备接收校验和
        if (receivedDataCount_ == dataLength_) {
            // LOG_DEBUG_FMT("接收数据 %zu/%d 完成", receivedDataCount_, dataLength_);
        }
        return false;
    }

    // 5. 接收校验和并验证
    if (!isChecksumReceived_) {
        frameBuffer_.push_back(byte);
        isChecksumReceived_ = true;
        // 计算校验和（范围：帧头后index 2 到 数据部分结束）
        size_t checksumStart = 2;                      // 帧头后第1字节（跳过帧头2字节）
        size_t checksumEnd = frameBuffer_.size() - 1;  // 校验和前（当前最后一个字节是校验和）
        uint8_t calculatedChecksum = 0;
        for (size_t i = checksumStart; i < checksumEnd; ++i) {
            calculatedChecksum += frameBuffer_[i];
        }
        calculatedChecksum &= 0xFF;  // 取低8位

        // 验证校验和
        if (calculatedChecksum != byte) {
            LOG_ERROR_FMT("校验和不匹配，计算值:0x%02X，接收值:0x%02X", calculatedChecksum, byte);
            reset();
            return false;
        } else {
            // LOG_DEBUG("校验和匹配");
        }
        return false;
    }

    // 6. 接收帧尾（2字节）并验证
    frameBuffer_.push_back(byte);
    static const std::vector<uint8_t> tail = GB_FRAME_TAIL;
    // 确保帧尾长度为2字节（根据协议定义）
    if (tail.size() != 2) {
        LOG_ERROR_FMT("帧尾定义错误，必须为2字节，实际:%zu字节", tail.size());
        reset();
        return false;
    }
    // 检查是否已接收到完整帧尾
    if (frameBuffer_.size() >= headerMatchCount_ + 24 + 1 + dataLength_ + 1 + 2) {  // 总长度校验
        // 提取接收到的帧尾（最后2字节）
        size_t tailStart = frameBuffer_.size() - 2;
        bool tailMatched = (frameBuffer_[tailStart] == tail[0]) && (frameBuffer_[tailStart + 1] == tail[1]);
        if (!tailMatched) {
            LOG_ERROR_FMT("帧尾不匹配，预期:0x%02X 0x%02X，实际:0x%02X 0x%02X", tail[0], tail[1], frameBuffer_[tailStart], frameBuffer_[tailStart + 1]);
            reset();
            return false;
        }
        // LOG_DEBUG_FMT("帧尾[0x%02X,0x%02X]验证通过，帧组装完成。", frameBuffer_[tailStart], frameBuffer_[tailStart + 1]);
        isFrameComplete_ = true;
        return true;  // 整个帧组装完成
    }

    return false;
}