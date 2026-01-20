#include "MsgNodeSend.h"

#include <cstring>
#include <stdexcept>

#include "../logs/Logger.h"

MsgNodeSend::MsgNodeSend(const char* msg, size_t max_len, int msg_id, const std::string& uuid)
    : m_msgNodeSend_curLen(0), m_msgNodeSend_totalLen(max_len), m_msgNodeSend_msgId(msg_id), m_msgNodeSend_UUID(uuid) {
  // 校验消息长度不超过最大限制
  size_t msg_len = max_len;

  // 缓冲区大小为max_len + 1（仅保留字符串结束符空间）
  m_msgNodeSend_data = new char[m_msgNodeSend_totalLen + 1]();
  m_msgNodeSend_data[m_msgNodeSend_totalLen] = '\0';

  // 直接写入原始消息数据（不添加帧头帧尾）
  memcpy(m_msgNodeSend_data, msg, msg_len);
  // 更新当前长度为实际消息长度
  m_msgNodeSend_curLen = msg_len;
}

MsgNodeSend::~MsgNodeSend() {
  delete[] m_msgNodeSend_data;
  LOG_WARNING_FMT("[%s]发送节点已销毁 消息ID %hd", m_msgNodeSend_UUID.c_str(), m_msgNodeSend_msgId);
}