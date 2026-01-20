#pragma once
#ifndef FRAMES_I_FRAME_PARSER_H
#define FRAMES_I_FRAME_PARSER_H

#include <memory>
#include <string>
#include <vector>

// 前向声明帧数据结构
struct FrameBase {
  virtual ~FrameBase() = default;
  std::string protocol;          // 协议类型
  int msgId;                     // 消息ID
  std::vector<uint8_t> rawData;  // 原始帧数据
};

// 帧解析器接口
class IFrameParser {
 public:
  virtual ~IFrameParser() = default;

  // 检查帧是否完整（基于协议结构特征）
  virtual bool isFrameComplete(const std::vector<uint8_t>& frame) const = 0;
  // 解析帧数据
  virtual bool parse(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg) = 0;
  // 保存帧到数据库
  virtual bool saveToDB(const FrameBase& frame, const std::string& sessionUuid) = 0;
  // 获取协议对应的帧头（支持多字节）
  virtual const std::vector<uint8_t>& getFrameHeader() const = 0;
  // 获取协议对应的帧尾（支持多字节）
  virtual const std::vector<uint8_t>& getFrameTail() const = 0;
};

#endif  // FRAMES_I_FRAME_PARSER_H