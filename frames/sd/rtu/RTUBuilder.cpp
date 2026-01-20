#include "RTUBuilder.h"

#include <sstream>

#include "../../../configs/frames/sd/RTUConfig.h"
#include "../../../logs/Logger.h"  // 假设存在日志工具类
#include "RTUParser.h"

// 构建 注册帧-反馈帧
std::vector<uint8_t> FrameRTUBuilder::frameRTUResponderRegister(const FrameRTURegister& originalFrame) {
  std::vector<uint8_t> frameRTUBuilder;

  // 1·帧头（1字节，固定0x7B）
  frameRTUBuilder.push_back(originalFrame.header);

  // 2·业务流水号（复用-原流水号，1字节）
  frameRTUBuilder.push_back(originalFrame.serialNumber);

  // 3·目标地址（复用-源地址，2字节，实现原路返回）
  frameRTUBuilder.push_back((originalFrame.sourceAddress >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.sourceAddress & 0xFF);         // 低8位

  // 4·源地址（复用-目标地址，2字节）
  frameRTUBuilder.push_back((originalFrame.targetAddress >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.targetAddress & 0xFF);         // 低8位

  // 5·索引（复用-原索引，2字节）
  frameRTUBuilder.push_back((originalFrame.index >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.index & 0xFF);         // 低8位

  //  6·固定填充（复用-原固定填充，7字节）
  for (int i = 0; i < 7; i++) {
    frameRTUBuilder.push_back(originalFrame.fixedPadding[i]);
  }

  // 7·设备编号（复用-原设备编号，12字节）
  for (char c : originalFrame.deviceNumber) {
    frameRTUBuilder.push_back(static_cast<uint8_t>(c));
  }

  // 8·数据长度（2字节）
  frameRTUBuilder.push_back((originalFrame.dataLength >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.dataLength & 0xFF);         // 低8位

  // 9·服务器当前时间（7字节，BCD码）
  RTUParser rtuParser;
  auto serverTimeBCD = rtuParser.getCurrentTimeBCD();  // 获取服务器当前时间
  for (uint8_t b : serverTimeBCD) {
    frameRTUBuilder.push_back(b);
  }

  // 10·校验和（1字节）
  uint8_t checksum = 0;
  for (size_t i = 1; i < frameRTUBuilder.size(); ++i) {
    checksum += frameRTUBuilder[i];
  }
  checksum &= 0xFF;  // 只取最低8位
  frameRTUBuilder.push_back(checksum);

  // 11·帧尾（1字节，固定0x7D）
  frameRTUBuilder.push_back(originalFrame.tail);

  return frameRTUBuilder;
}

// 构建 数据帧-反馈帧
std::vector<uint8_t> FrameRTUBuilder::frameRTUResponderReportData(const FrameRTUReportData& originalFrame) {
  std::vector<uint8_t> frameRTUBuilder;

  // 1·帧头（1字节，固定0x7B）
  frameRTUBuilder.push_back(originalFrame.header);

  // 2·业务流水号（复用-原流水号，1字节）
  frameRTUBuilder.push_back(originalFrame.serialNumber);

  // 3·目标地址（复用-源地址，2字节，实现原路返回）
  frameRTUBuilder.push_back((originalFrame.sourceAddress >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.sourceAddress & 0xFF);         // 低8位

  // 4·源地址（复用-目标地址，2字节）
  frameRTUBuilder.push_back((originalFrame.targetAddress >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.targetAddress & 0xFF);         // 低8位

  // 5·索引（复用-原索引，2字节）
  frameRTUBuilder.push_back((originalFrame.index >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.index & 0xFF);         // 低8位

  //  6·固定填充（复用-原固定填充，7字节）
  for (int i = 0; i < 7; i++) {
    frameRTUBuilder.push_back(originalFrame.fixedPadding[i]);
  }

  // 7·设备编号（复用-原设备编号，12字节）
  for (char c : originalFrame.deviceNumber) {
    frameRTUBuilder.push_back(static_cast<uint8_t>(c));
  }

  // 8·数据长度（2字节）
  frameRTUBuilder.push_back((originalFrame.dataLength >> 8) & 0xFF);  // 高8位
  frameRTUBuilder.push_back(originalFrame.dataLength & 0xFF);         // 低8位

  // 9·成功标志（1字节，固定0x01）
  frameRTUBuilder.push_back(0x01);

  // 10·校验和（1字节）
  uint8_t checksum = 0;
  for (size_t i = 1; i < frameRTUBuilder.size(); ++i) {
    checksum += frameRTUBuilder[i];
  }
  checksum &= 0xFF;  // 只取最低8位
  frameRTUBuilder.push_back(checksum);

  // 11·帧尾（1字节，固定0x7D）
  frameRTUBuilder.push_back(originalFrame.tail);

  return frameRTUBuilder;
}