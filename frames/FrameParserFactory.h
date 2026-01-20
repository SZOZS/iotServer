#pragma once
#ifndef FRAME_PARSER_FACTORY_H
#define FRAME_PARSER_FACTORY_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "IFrameParser.h"

class FrameParserFactory {
 public:
  using Creator = std::function<std::unique_ptr<IFrameParser>()>;

  // 注册解析器
  static void registerParser(const std::string& protocol, const std::vector<uint8_t>& header, Creator creator) { getCreators().emplace(protocol, std::make_pair(header, creator)); }

  // 根据协议类型创建解析器
  static std::unique_ptr<IFrameParser> createParser(const std::string& protocol) {
    auto it = getCreators().find(protocol);
    if (it != getCreators().end()) {
      return it->second.second();
    }
    return nullptr;
  }

  // 获取所有协议的帧头信息
  static const std::unordered_map<std::string, std::pair<std::vector<uint8_t>, Creator>>& getRegisteredProtocols() { return getCreators(); }

 private:
  static std::unordered_map<std::string, std::pair<std::vector<uint8_t>, Creator>>& getCreators() {
    static std::unordered_map<std::string, std::pair<std::vector<uint8_t>, Creator>> creators;
    return creators;
  }
};

#endif  // FRAME_PARSER_FACTORY_H