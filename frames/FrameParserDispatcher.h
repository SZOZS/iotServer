#pragma once
#ifndef FRAME_PARSER_DISPATCHER_H
#define FRAME_PARSER_DISPATCHER_H

#include <string>
#include <unordered_map>
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
            const auto& info = pair.second;            // 协议信息（帧头+其他配置）
            const auto& header = info.first;           // 帧头字节数组

            // 帧长度不足帧头长度，跳过
            if (frame.size() < header.size()) {
                continue;
            }

            // 逐字节匹配帧头
            bool match = true;
            for (size_t i = 0; i < header.size(); ++i) {
                if (frame[i] != header[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                LOG_DEBUG_FMT_DELAY("识别到协议：%s（帧头匹配）", protocol.c_str());
                return protocol;
            }
        }
        LOG_WARNING_DELAY("未识别到任何匹配的协议");
        return "";
    }

    static std::string identifyProtocol(const std::vector<uint8_t>& frame, const std::vector<std::string>& limit_protocols)
    {
        // 空列表直接返回空（无匹配范围）
        if (limit_protocols.empty()) {
            LOG_WARNING_DELAY("限定协议列表为空，跳过协议识别");
            return "";
        }

        // 获取所有已注册协议的帧头配置
        const auto& all_protocols = FrameParserFactory::getRegisteredProtocols();

        // 仅遍历限定的子协议列表
        for (const std::string& target_proto : limit_protocols) {
            // 查找该子协议是否已注册
            auto proto_it = all_protocols.find(target_proto);
            if (proto_it == all_protocols.end()) {
                LOG_WARNING_FMT_DELAY("限定协议[%s]未注册，跳过匹配", target_proto.c_str());
                continue;
            }

            const auto& info = proto_it->second;
            const auto& header = info.first;

            // 帧长度不足帧头长度，跳过
            if (frame.size() < header.size()) {
                LOG_DEBUG_FMT_DELAY("帧长度[%zu]不足协议[%s]帧头长度[%zu]，跳过", frame.size(), target_proto.c_str(), header.size());
                continue;
            }

            // 逐字节匹配帧头
            bool match = true;
            for (size_t i = 0; i < header.size(); ++i) {
                if (frame[i] != header[i]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                LOG_INFO_FMT_DELAY("在限定列表中识别到协议：%s", target_proto.c_str());
                return target_proto;
            } else {
                LOG_DEBUG_FMT_DELAY("协议[%s]帧头不匹配，继续检查下一个", target_proto.c_str());
            }
        }

        LOG_WARNING_FMT_DELAY("在限定协议列表[%zu个]中未匹配到任何协议", limit_protocols.size());
        return "";
    }

    // 调度解析过程
    static bool dispatch(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg)
    {
        // 识别协议（遍历所有已注册协议）
        std::string protocol = identifyProtocol(frame);
        if (protocol.empty()) {
            LOG_ERROR_DELAY("未知协议类型");
            return false;
        }
        LOG_INFO_FMT_DELAY("识别到协议：%s，开始解析", protocol.c_str());

        // 创建解析器
        auto parser = FrameParserFactory::createParser(protocol);
        if (!parser) {
            LOG_ERROR_FMT_DELAY("未找到协议解析器:%s", protocol.c_str());
            return false;
        }
        LOG_INFO_DELAY("解析器正确");

        // 检查帧完整性
        if (!parser->isFrameComplete(frame)) {
            LOG_ERROR_DELAY("帧数据不完整");
            return false;
        }
        LOG_INFO_DELAY("帧数据完整");

        // 执行解析
        if (!parser->parse(frame, outFrame, errorMsg)) {
            LOG_ERROR_FMT_DELAY("协议[%s]解析失败：%s", protocol.c_str(), errorMsg.c_str());
            return false;
        }
        LOG_INFO_FMT_DELAY("协议[%s]解析成功", protocol.c_str());
        // 执行解析
        return true;
    }

    // 调度解析过程
    static bool dispatch(const std::vector<uint8_t>& frame,
                         std::unique_ptr<FrameBase>& outFrame,
                         std::string& errorMsg,
                         const std::vector<std::string>& limit_protocols)
    {
        // 识别协议（仅在限定列表内匹配）
        std::string protocol = identifyProtocol(frame, limit_protocols);
        if (protocol.empty()) {
            LOG_ERROR_DELAY("在限定协议列表中未识别到匹配协议");
            return false;
        }
        LOG_INFO_FMT_DELAY("在限定列表中识别到协议：%s，开始解析", protocol.c_str());

        // 创建解析器
        auto parser = FrameParserFactory::createParser(protocol);
        if (!parser) {
            LOG_ERROR_FMT_DELAY("未找到协议解析器:%s", protocol.c_str());
            return false;
        }
        LOG_INFO_DELAY("解析器正确");

        // 检查帧完整性
        if (!parser->isFrameComplete(frame)) {
            LOG_ERROR_DELAY("帧数据不完整");
            return false;
        }
        LOG_INFO_DELAY("帧数据完整");

        // 执行解析
        if (!parser->parse(frame, outFrame, errorMsg)) {
            LOG_ERROR_FMT_DELAY("协议[%s]解析失败：%s", protocol.c_str(), errorMsg.c_str());
            return false;
        }
        LOG_INFO_FMT_DELAY("协议[%s]解析成功", protocol.c_str());
        // 执行解析
        return true;
    }
};

#endif  // FRAME_PARSER_DISPATCHER_H