#pragma once
#ifndef FRAME_ASSEMBLER_FACTORY_H
#define FRAME_ASSEMBLER_FACTORY_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "IFrameAssembler.h"

class FrameAssemblerFactory {
 public:
  using Creator = std::function<std::unique_ptr<IFrameAssembler>()>;

  // 注册组装器（协议名 + 帧头 + 创建函数）
  static void registerAssembler(const std::string& protocol, const std::vector<uint8_t>& header, Creator creator) {
    getCreators().emplace(protocol, std::make_pair(header, creator));
  }

  // 根据协议创建组装器
  static std::unique_ptr<IFrameAssembler> createAssembler(const std::string& protocol) {
    auto it = getCreators().find(protocol);
    if (it != getCreators().end()) {
      return it->second.second();
    }
    return nullptr;
  }

  // 获取所有协议的帧头信息（用于协议识别）
  static const std::unordered_map<std::string, std::pair<std::vector<uint8_t>, Creator>>& getRegisteredProtocols() { return getCreators(); }

 private:
  static std::unordered_map<std::string, std::pair<std::vector<uint8_t>, Creator>>& getCreators() {
    static std::unordered_map<std::string, std::pair<std::vector<uint8_t>, Creator>> creators;
    return creators;
  }
};

#endif  // FRAME_ASSEMBLER_FACTORY_H