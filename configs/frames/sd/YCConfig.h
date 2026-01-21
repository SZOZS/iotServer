#pragma once
#ifndef FRAME_SD_YC_CONFIG_H
#define FRAME_SD_YC_CONFIG_H

enum YCMsgId
{
    GB_MSG_STATUS = 0x01,     // 状态信息帧
    GB_MSG_ALARM = 0x02,      // 报警信息帧
    GB_MSG_CONTROL = 0x03,    // 控制指令帧
    GB_MSG_HEARTBEAT = 0x04,  // 心跳帧

    // 0x00 预留
    // 0x01 控制命令 - 时间同步
    // 0x02 发送数据 - 发送火灾报警和建筑消防设施运行状态等信息
    // 0x03 确认 - 对控制命令和发送信息的确认回答
    // 0x04 请求 - 查询火灾报警和建筑消防设施运行状态等信息
    // 0x05 应答 - 返回查询的信息
    // 0x06 否认 - 对控制命令和发送信息的否认回答
    // 0x07~0x7F - 7~127 预留
    // 0x80~0xFF - 128~255 用户自行定义
    MSG_SD_YC_CONTROL_COMMAND = 90100,  // 控制命令 - 时间同步
    MSG_SD_YC_SEND_DATA = 90200,        // 发送数据 - 发送火灾报警和建筑消防设施运行状态等信息
    MSG_SD_YC_SEND_DATA_01 = 90201,     // 8.3.1.1 上传建筑消防设施系统状态 - 1
    MSG_SD_YC_SEND_DATA_02 = 90202,     // 8.3.1.2 上传建筑消防设施部件运行状态 - 2
    MSG_SD_YC_SEND_DATA_03 = 90203,     // 8.3.1.3 上传建筑消防设施部件模拟量值 - 3
    MSG_SD_YC_SEND_DATA_04 = 90204,     // 8.3.1.4 上传建筑消防设施操作信息记录 - 4
    MSG_SD_YC_SEND_DATA_05 = 90205,     // 8.3.1.5 上传建筑消防设施软件版本 - 5
    MSG_SD_YC_SEND_DATA_06 = 90206,     // 8.3.1.6 上传建筑消防设施系统配置情况 - 6
    MSG_SD_YC_SEND_DATA_07 = 90207,     // 8.3.1.7 上传建筑消防设施部件配置情况 - 7
    MSG_SD_YC_SEND_DATA_08 = 90208,     // 8.3.1.8 上传建筑消防设施系统时间 - 8
    MSG_SD_YC_SEND_DATA_21 = 90221,     // 8.3.1.9 上传用户信息传输装置运行状态 - 21
    MSG_SD_YC_SEND_DATA_24 = 90224,     // 8.3.1.10 上传用户信息传输装置操作信息记录 - 24
    MSG_SD_YC_SEND_DATA_25 = 90225,     // 8.3.1.11 上传用户信息传输装置软件版本 - 25
    MSG_SD_YC_SEND_DATA_26 = 90226,     // 8.3.1.12 上传用户信息传输装置配置情况 - 26
    MSG_SD_YC_SEND_DATA_28 = 90228,     // 8.3.1.13 上传用户信息传输装置系统时间 - 28

    MSG_SD_YC_CONFIRM = 90300,   // 确认 - 对控制命令和发送信息的确认回答
    MSG_SD_YC_REQUEST = 90400,   // 请求 - 查询火灾报警和建筑消防设施运行状态等信息
    MSG_SD_YC_RESPONSE = 90500,  // 应答 - 返回查询的信息
    MSG_SD_YC_DENY = 90600,      // 否认 - 对控制命令和发送信息的否认回答
};

#define GB_MSG_SEND_DATA_01 0x01  // 上传建筑消防设施系统状态
#define GB_MSG_SEND_DATA_02 0x02  // 上传建筑消防设施部件运行状态
#define GB_MSG_SEND_DATA_03 0x03  // 上传建筑消防设施部件模拟量值
#define GB_MSG_SEND_DATA_04 0x04  // 上传建筑消防设施操作信息记录
#define GB_MSG_SEND_DATA_05 0x05  // 上传建筑消防设施软件版本
#define GB_MSG_SEND_DATA_06 0x06  // 上传建筑消防设施系统配置情况
#define GB_MSG_SEND_DATA_07 0x07  // 上传建筑消防设施部件配置情况
#define GB_MSG_SEND_DATA_08 0x08  // 上传建筑消防设施系统时间
#define GB_MSG_SEND_DATA_21 0x15  // 上传用户信息传输装置运行状态
#define GB_MSG_SEND_DATA_24 0x18  // 上传用户信息传输装置操作信息记录
#define GB_MSG_SEND_DATA_25 0x19  // 上传用户信息传输装置软件版本
#define GB_MSG_SEND_DATA_26 0x1a  // 上传用户信息传输装置配置情况
#define GB_MSG_SEND_DATA_28 0x1c  // 上传用户信息传输装置系统时间

// 国标GB/T26875.3帧常量
#define GB_FRAME_HEADER {0x40, 0x40}  // 帧头
#define GB_FRAME_TAIL {0x23, 0x23}    // 帧尾
#define GB_MAX_FRAME_LEN 1024 * 10    // 最大帧长度
#define GB_DEVICE_ADDR_LEN 6          // 设备地址长度（字节）
#define GB_CRC_LEN 2                  // CRC校验长度（字节）

#endif  // FRAME_SD_YC_CONFIG_H
