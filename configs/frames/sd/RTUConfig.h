#pragma once
#ifndef FRAME_SD_RTU_CONFIG_H
#define FRAME_SD_RTU_CONFIG_H

#define MAX_LENGTH 1024 * 2   // 最大数据长度（2KB），限制单条消息的数据部分大小
#define HEAD_TOTAL_LEN 4      // 消息头部总长度（4字节）：包含消息ID（2字节）和数据长度（2字节）
#define HEAD_ID_LEN 2         // 消息头部中消息ID的长度（2字节，网络字节序）
#define HEAD_DATA_LEN 2       // 消息头部中数据长度的长度（2字节，网络字节序）
#define MAX_RECV_QUEUE 10000  // 最大接收队列大小（10000条），防止消息堆积导致内存溢出
#define MAX_SEND_QUEUE 1000   // 最大发送队列大小（1000条），限制单个会话的待发送消息数量

enum RTUMsgIds {
  MSG_SD_RTU_REGISTER = 10001,              // 注册帧
  MSG_SD_RTU_REGISTER_FEEDBACK = 11001,     // 注册帧-反馈
  MSG_SD_RTU_REPORT_DATA = 10003,           // 数据帧
  MSG_SD_RTU_REPORT_DATA_FEEDBACK = 11003,  // 数据帧-反馈
};

#define FRAME_SD_RTU_START 0x7B               // 帧起始符（十六进制7B，对应'{'）
#define FRAME_SD_RTU_END 0x7D                 // 帧结束符（十六进制7D，对应'}'）
#define FRAME_SD_RTU_REGISTER_DATA_LEN 7      // 注册帧数据长度
#define FRAME_SD_RTU_REPORT_DATA_DATA_LEN 43  // 数据帧数据长度
#define FRAME_TYPE_REGISTER_LEN 38
#define FRAME_TYPE_REPORT_DATA_LEN 74

#define MAX_FRAME_LEN 1024 * 10  // 最大帧长度（10KB，根据实际需求调整）
#define FRAME_RTU_MAX_LEN 1024   // 最大帧长度（1KB，根据实际需求调整）

#endif  // FRAME_SD_RTU_CONFIG_H