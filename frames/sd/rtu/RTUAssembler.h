#pragma once
#include "../../../configs/frames/sd/RTUConfig.h"
#include "../../FrameAssembler.h"
#include "../../IFrameAssembler.h"
#include "RTUBuilder.h"
#include "RTUParser.h"

// 修正类名拼写
class RTUAssembler : public IFrameAssembler {
 public:
  RTUAssembler() { reset(); }

  // 实现IFrameAssembler的纯虚函数
  std::vector<uint8_t> assembleResponse(const FrameBase& requestFrame) override {
    // 可根据请求帧生成响应（参考RTUBuilder）
    if (requestFrame.msgId == MSG_SD_RTU_REGISTER) {
      const auto& regFrame = static_cast<const FrameRTURegister&>(requestFrame);
      return FrameRTUBuilder::frameRTUResponderRegister(regFrame);
    } else if (requestFrame.msgId == MSG_SD_RTU_REPORT_DATA) {
      const auto& dataFrame = static_cast<const FrameRTUReportData&>(requestFrame);
      return FrameRTUBuilder::frameRTUResponderReportData(dataFrame);
    }
    return {};
  }
  std::string getProtocol() const override { return "SD_RTU"; }
  uint8_t getHeader() const { return FRAME_SD_RTU_START; }
  bool processByte(uint8_t byte) override;
  std::vector<uint8_t> getFrame() const override { return frameBuffer_; }
  void reset() override;

 private:
  std::vector<uint8_t> frameBuffer_;  // 帧数据缓冲区
  bool isHeaderFound_;                // 是否找到帧头
  bool isFrameComplete_;              // 帧是否组装完成
  bool isDataLengthParsed_;           // 是否已解析27/28字节的后续数据长度
  uint16_t dataLength_;               // 从27/28字节解析出的后续数据长度
  size_t remainingDataToReceive_;     // 还需接收的后续数据字节数
  bool isDataReceived_;               // 是否已接收完后续数据
  bool isChecksumReceived_;           // 是否已接收校验和
  uint8_t checksum_;                  // 存储校验和
};