#pragma once
#include <cstdint>
#include <string>
#include <vector>

// 帧组装器基类：定义所有协议帧组装的统一接口
class FrameAssembler
{
public:
    virtual ~FrameAssembler() = default;
    // 处理接收到的字节流，返回是否组装出完整帧
    virtual bool processByte(uint8_t byte) = 0;
    // 获取组装完成的帧字节流数组
    virtual std::vector<uint8_t> getFrame() const = 0;
    // 重置组装状态（用于帧处理完成或异常时）
    virtual void reset() = 0;
    // 获取当前协议的帧头标识(用于协议识别)
    virtual uint8_t getHeader() const = 0;
};