#pragma once
#ifndef MSG_NODE_SEND_H
#define MSG_NODE_SEND_H

#include <string>

#include "../configs/frames/sd/RTUConfig.h"

class LogicSystem;

// 发送消息节点：负责发送消息的缓冲区管理和消息组装
class MsgNodeSend
{
    friend class LogicSystem;  // 允许业务逻辑系统访问私有成员

public:
    // 构造函数：初始化发送消息（数据内容+长度+消息ID）
    MsgNodeSend(const char* msg, size_t max_len, int msg_id, const std::string& uuid);

    // 析构函数
    ~MsgNodeSend();

    size_t m_msgNodeSend_curLen;    // 当前已写入的长度
    size_t m_msgNodeSend_totalLen;  // 消息总长度
    char* m_msgNodeSend_data;       // 消息数据缓冲区

private:
    int m_msgNodeSend_msgId;         // 消息ID（标识消息类型）
    std::string m_msgNodeSend_UUID;  // 会话唯一标识
};

#endif  // MSG_NODE_SEND_H