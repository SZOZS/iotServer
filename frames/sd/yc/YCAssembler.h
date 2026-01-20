#pragma once
#include "../../../configs/frames/sd/YCConfig.h"
#include "../../FrameAssembler.h"
#include "../../IFrameAssembler.h"
#include "YCParser.h"
#include "YCBuilder.h"

class YCAssembler : public IFrameAssembler {
 public:
  YCAssembler() { reset(); }

  // 实现IFrameAssembler的纯虚函数
  std::vector<uint8_t> assembleResponse(const FrameBase& requestFrame) override {
    // 转换为FrameGB类型（所有SD_YC帧均继承自FrameGB）
    const FrameGB& gbFrame = static_cast<const FrameGB&>(requestFrame);
    // 可根据请求帧生成响应（参考YCParser的帧结构）
    if (requestFrame.msgId == MSG_SD_YC_SEND_DATA_01 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_02 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_03 ||
        requestFrame.msgId == MSG_SD_YC_SEND_DATA_04 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_05 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_06 ||
        requestFrame.msgId == MSG_SD_YC_SEND_DATA_07 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_08 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_21 ||
        requestFrame.msgId == MSG_SD_YC_SEND_DATA_24 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_25 || requestFrame.msgId == MSG_SD_YC_SEND_DATA_26 ||
        requestFrame.msgId == MSG_SD_YC_SEND_DATA_28) {
      // 构造确认帧
      return YCBuilder::buildConfirmFrame(gbFrame);
    } else {
      // 构造否认帧
      return YCBuilder::buildDenyFrame(gbFrame);
    }
    return {};
  }

  std::string getProtocol() const override { return "SD_YC"; }
  uint8_t getHeader() const {
    // 返回帧头第一个字节（用于协议识别）
    static const std::vector<uint8_t> header = GB_FRAME_HEADER;
    return header.empty() ? 0 : header[0];
  }
  bool processByte(uint8_t byte) override;
  std::vector<uint8_t> getFrame() const override { return frameBuffer_; }
  void reset() override;

 private:
  std::vector<uint8_t> frameBuffer_;  // 帧数据缓冲区
  bool isHeaderFound_;                // 是否已找到帧头
  bool isDataLengthParsed_;           // 是否已解析数据长度（替换原isFrameLengthParsed_）
  uint16_t dataLength_;               // 数据长度（从24-25字节解析）
  size_t remainingDataToReceive_;     // 待接收的数据字节数
  bool isCommandByteValidated_;       // 命令字节是否验证通过
  size_t receivedDataCount_;          // 已接收的数据字节数
  bool isChecksumReceived_;           // 是否已接收到校验和
  bool isFrameComplete_;              // 帧是否组装完成
  size_t headerMatchCount_;           // 记录已匹配的帧头字节数
};
