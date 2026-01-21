#pragma once
#ifndef FRAME_PARSER_DISPATCHER_H
#define FRAME_PARSER_DISPATCHER_H

#include <string>
#include <vector>

#include "../logs/Logger.h"

#include "FrameParserFactory.h"
#include "IFrameParser.h"

class FrameParserDispatcher
{
public:
    // 识别协议类型（支持多字节帧头）
    static std::string identifyProtocol(const std::vector<uint8_t>& frame)
    {
        const auto& protocols = FrameParserFactory::getRegisteredProtocols();
        // 替换结构化绑定为显式访问键值对的first和second
        for (const auto& pair : protocols) {
            const std::string& protocol = pair.first;  // 显式获取协议名（键）
            const auto& info = pair.second;            // 显式获取协议信息（值）
            const auto& header = info.first;
            if (frame.size() >= header.size()) {
                bool match = true;
                for (size_t i = 0; i < header.size(); ++i) {
                    if (frame[i] != header[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return protocol;
                }
            }
        }
        return "";
    }

    // 调度解析过程
    static bool dispatch(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg)
    {
        // 识别协议
        std::string protocol = identifyProtocol(frame);
        if (protocol.empty()) {
            LOG_ERROR("未知协议类型");
            return false;
        }
        LOG_INFO("协议正确");

        // 创建解析器
        auto parser = FrameParserFactory::createParser(protocol);
        if (!parser) {
            LOG_ERROR_FMT("未找到协议解析器:%s", protocol.c_str());
            return false;
        }
        LOG_INFO("解析器正确");

        // 检查帧完整性
        if (!parser->isFrameComplete(frame)) {
            LOG_ERROR("帧数据不完整");
            return false;
        }
        LOG_INFO("帧数据完整");

        // 执行解析
        return parser->parse(frame, outFrame, errorMsg);
    }
};

#endif  // FRAME_PARSER_DISPATCHER_H