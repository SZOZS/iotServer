#pragma once
#ifndef I_FRAME_ASSEMBLER_H
#define I_FRAME_ASSEMBLER_H

#include <vector>

#include "IFrameParser.h"

class IFrameAssembler
{
public:
    virtual ~IFrameAssembler() = default;

    virtual bool processByte(uint8_t byte) = 0;                                        // 处理单字节，返回是否组装完成
    virtual std::vector<uint8_t> getFrame() const = 0;                                 // 获取组装完成的帧
    virtual void reset() = 0;                                                          // 重置组装状态
    virtual std::vector<uint8_t> assembleResponse(const FrameBase& requestFrame) = 0;  // 组装响应帧（根据输入帧生成响应）
    virtual std::string getProtocol() const = 0;                                       // 获取支持的协议名
};

#endif  // I_FRAME_ASSEMBLER_H