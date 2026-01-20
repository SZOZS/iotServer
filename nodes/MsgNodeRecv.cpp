#include "MsgNodeRecv.h"

#include <cstring>

#include "../logs/Logger.h"

// 构造函数实现
MsgNodeRecv::MsgNodeRecv(size_t max_frame_len, short msg_id, const std::string& uuid)
    : m_msgNodeRecv_curLen(0),                // 当前已读取/写入的长度
      m_msgNodeRecv_totalLen(max_frame_len),  // 消息总长度
      m_msgNodeRecv_msgId(msg_id),            // 消息ID（标识消息类型）
      m_msgNodeRecv_foundStart(false),        // 是否找到帧起始符-思迪RTU
      m_msgNodeRecv_foundEnd(false),          // 是否找到帧结束符-思迪RTU
      m_msgNodeRecv_UUID(uuid)                // 会话id
{
  // 分配缓冲区（+1用于存储字符串结束符）
  m_msgNodeRecv_data = new char[m_msgNodeRecv_totalLen + 1]();
  m_msgNodeRecv_data[m_msgNodeRecv_totalLen] = '\0';
}

// 析构函数实现
MsgNodeRecv::~MsgNodeRecv() {
  delete[] m_msgNodeRecv_data;
  LOG_WARNING_FMT("[%s]接收节点已销毁 消息ID %hd", m_msgNodeRecv_UUID.c_str(), m_msgNodeRecv_msgId);
}

// 清空缓冲区实现
void MsgNodeRecv::clear() {
  std::memset(m_msgNodeRecv_data, 0, m_msgNodeRecv_totalLen);  // 清空数据
  m_msgNodeRecv_curLen = 0;                                    // 重置当前长度
}