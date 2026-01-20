#pragma once
#ifndef FRAME_SD_YC_BUILDER_H
#define FRAME_SD_YC_BUILDER_H

#include <array>
#include <cstdint>
#include <vector>

#include "../../../configs/frames/sd/YCConfig.h"
#include "YCParser.h"

// 确认帧结构体（commandByte = 0x03）
struct FrameConfirm : public FrameGB {
  FrameConfirm(const FrameGB& base) : FrameGB(base) {
    commandByte = 0x03;  // 确认命令字节
    dataLength = 0;      // 无应用数据
  }
};

// 否认帧结构体（commandByte = 0x06）
struct FrameDeny : public FrameGB {
  FrameDeny(const FrameGB& base) : FrameGB(base) {
    commandByte = 0x06;  // 否认命令字节
    dataLength = 0;      // 无应用数据
  }
};

class YCBuilder {
 public:
  // 构建确认帧（基于请求帧生成）
  static std::vector<uint8_t> buildConfirmFrame(const FrameGB& requestFrame);

  // 构建否认帧（基于请求帧生成）
  static std::vector<uint8_t> buildDenyFrame(const FrameGB& requestFrame);

 private:
  // 通用帧构建辅助函数
  static std::vector<uint8_t> buildFrame(const FrameGB& baseFrame);

  // 计算校验和（控制单元+应用数据算术和，取低8位）
  static uint8_t calculateChecksum(const std::vector<uint8_t>& frameData);
};

#endif  // FRAME_SD_YC_BUILDER_H