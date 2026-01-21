#pragma once
#ifndef MSG_NODE_RECV_H
#define MSG_NODE_RECV_H

#include <string>

#include "../configs/frames/sd/RTUConfig.h"

class LogicSystem;
class CSession;

// 接收消息节点：负责接收消息的缓冲区管理和帧解析状态
class MsgNodeRecv
{
    friend class LogicSystem;  // 允许业务逻辑系统访问私有成员
    friend class CSession;     // 允许会话类访问私有成员

public:
    // 构造函数：初始化接收消息缓冲区
    MsgNodeRecv(size_t max_frame_len, short msg_id, const std::string& uuid);

    // 析构函数：释放缓冲区
    ~MsgNodeRecv();

    // 清空消息缓冲区
    void clear();

    size_t m_msgNodeRecv_curLen;    // 当前已读取/写入的长度
    size_t m_msgNodeRecv_totalLen;  // 消息总长度
    char* m_msgNodeRecv_data;       // 消息数据缓冲区

private:
    short m_msgNodeRecv_msgId;           // 消息ID（标识消息类型）
    bool m_msgNodeRecv_foundStart;       // 是否找到帧起始符-思迪RTU
    bool m_msgNodeRecv_foundEnd;         // 是否找到帧结束符-思迪RTU
    std::string m_msgNodeRecv_frameHex;  // 存储完整帧的十六进制字符串
    std::string m_msgNodeRecv_UUID;      // 会话唯一标识
};

#endif  // MSG_NODE_RECV_H