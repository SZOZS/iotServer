#pragma once
#ifndef FRAME_SD_YC_PARSER_H
#define FRAME_SD_YC_PARSER_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../../configs/frames/sd/YCConfig.h"
#include "../../FrameParserFactory.h"
#include "../../IFrameParser.h"

// 基类：公共帧结构
struct FrameGB : public FrameBase
{
    std::array<uint8_t, 2> header;         // 帧头(2字节) - 第1~2字节 - 固定0x40 0x40
    uint16_t serialNumber;                 // 业务流水号(2字节) - 第3~4字节 - 原始值(低字节在前，解析时需处理字节序)
    std::array<uint8_t, 2> version;        // 协议版本号(2字节) - 第5~6字节 - 2字节，格式：主版本号(固定为1)，用户版本号(自定义)
    std::array<uint8_t, 6> timeTag;        // 时间标签(6字节) - 第7~12字节 - 6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年
    std::array<uint8_t, 6> sourceAddress;  // 源地址(6字节) - 第13~18字节(低字节在前)
    std::array<uint8_t, 6> targetAddress;  // 目的地址(6字节) - 第19~24字节(低字节在前)
    uint16_t dataLength;                   // 应用数据单元长度(2字节) - 第25~26字节(低字节在前，≤1024)
    uint8_t commandByte;                   // 命令字节(1字节) - 第27字节(标识控制单元类型)
    // 各派生类
    uint8_t checksum;             // 校验和(1字节) - 倒数第3字节(控制单元+应用数据算术和，取低8位)
    std::array<uint8_t, 2> tail;  // 帧尾(2字节) - 最后2字节，固定0x23 0x23
};

// 派生类 - 发送数据 - 发送火灾报警和建筑消防设施运行状态等信息 - MSG_SD_YC_SEND_DATA
struct FrameSendData01 : public FrameGB
{                                                            // 8.3.1.1 上传建筑消防设施系统状态 - typeIdentifier(0x01)
    FrameSendData01(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x01;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于102)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                                // 信息对象结构体(根据objCount的值存在多个)
        uint8_t systemType;                          // 系统类型(1字节)
        uint8_t systemAddress;                       // 系统地址(1字节)
        uint16_t systemStatus;                       // 系统状态(2字节)
        std::array<uint8_t, 6> statuOccurrenceTime;  // 状态发生时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData02 : public FrameGB
{                                                            // 8.3.1.2 上传建筑消防设施部件运行状态 - typeIdentifier(0x02)
    FrameSendData02(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x02;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于22)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                                // 信息对象结构体(根据objCount的值存在多个)
        uint8_t systemType;                          // 系统类型(1字节)
        uint8_t systemAddress;                       // 系统地址(1字节)
        uint8_t componentType;                       // 部件类型(1字节)
        uint32_t componentAddress;                   // 部件地址(4字节)
        uint16_t componentStatus;                    // 部件状态(2字节)
        std::array<uint8_t, 31> componentDesc;       // 部件说明(31字节)
        std::array<uint8_t, 6> statuOccurrenceTime;  // 状态发生时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData03 : public FrameGB
{                                                            // 8.3.1.3 上传建筑消防设施部件模拟量值 - typeIdentifier(0x03)
    FrameSendData03(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x03;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于63)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                                  // 信息对象结构体(根据objCount的值存在多个)
        uint8_t systemType;                            // 系统类型(1字节)
        uint8_t systemAddress;                         // 系统地址(1字节)
        uint8_t componentType;                         // 部件类型(1字节)
        uint32_t componentAddress;                     // 部件地址(4字节)
        uint8_t analogType;                            // 模拟量类型(1字节)
        uint16_t analogValue;                          // 模拟量值(2字节)
        std::array<uint8_t, 6> analogValueSampleTime;  // 模拟量值采样时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData04 : public FrameGB
{                                                            // 8.3.1.4 上传建筑消防设施操作信息记录 - typeIdentifier(0x04)
    FrameSendData04(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x04;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于102)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                                // 信息对象结构体(根据objCount的值存在多个)
        uint8_t systemType;                          // 系统类型(1字节)
        uint8_t systemAddress;                       // 系统地址(1字节)
        uint8_t operationInfo;                       // 操作信息(1字节)
        uint8_t operatorId;                          // 操作员编号(1字节)
        std::array<uint8_t, 6> operationRecordTime;  // 操作记录时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData05 : public FrameGB
{                                                            // 8.3.1.5 上传建筑消防设施软件版本 - typeIdentifier(0x05)
    FrameSendData05(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x05;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
    uint8_t systemType;                                      // 系统类型(1字节)
    uint8_t systemAddress;                                   // 系统地址(1字节)
    uint8_t softwareMajorVersion;                            // 软件主版本号(1字节)
    uint8_t softwareMinorVersion;                            // 软件次版本号(1字节)
};
struct FrameSendData06 : public FrameGB
{                                                            // 8.3.1.6 上传建筑消防设施系统配置情况 - typeIdentifier(0x06)
    FrameSendData06(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x06;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于3)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                     // 信息对象结构体(根据objCount的值存在多个)
        uint8_t systemType;               // 系统类型(1字节)
        uint8_t systemAddress;            // 系统地址(1字节)
        uint8_t systemDescLength;         // 系统说明长度(1字节，n为后续系统说明的字节数)
        std::vector<uint8_t> systemDesc;  // 系统说明(n字节，长度由systemDescLength指定)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData07 : public FrameGB
{                                                            // 8.3.1.7 上传建筑消防设施部件配置情况 - typeIdentifier(0x07)
    FrameSendData07(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x07;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于26)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                           // 信息对象结构体(根据objCount的值存在多个)
        uint8_t systemType;                     // 系统类型(1字节)
        uint8_t systemAddress;                  // 系统地址(1字节)
        uint8_t componentType;                  // 部件类型(1字节)
        uint32_t componentAddress;              // 部件地址(4字节)
        std::array<uint8_t, 31> componentDesc;  // 部件说明(31字节，固定长度)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData08 : public FrameGB
{                                                            // 8.3.1.8 上传建筑消防设施系统时间 - typeIdentifier(0x08)
    FrameSendData08(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x08;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
    uint8_t systemType;                                      // 系统类型(1字节)
    uint8_t systemAddress;                                   // 系统地址(1字节)
    std::array<uint8_t, 6> buildingFireFacilitySysTime;      // 建筑消防设施的系统时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
};
struct FrameSendData21 : public FrameGB
{                                                            // 8.3.1.9 上传用户信息传输装置运行状态 - typeIdentifier(0x15)
    FrameSendData21(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x15;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
    uint8_t state;                                           // 状态(1字节，具体状态值需参考协议定义)
    std::array<uint8_t, 6> statuOccurrenceTime;              // 状态发生时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
};
struct FrameSendData24 : public FrameGB
{                                                            // 8.3.1.10 上传用户信息传输装置操作信息记录 - typeIdentifier(0x18)
    FrameSendData24(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x18;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于127)，后面就有几个形同结构的结构体
    struct InfoObject
    {                                                // 信息对象结构体(根据objCount的值存在多个)
        uint8_t operationInfo;                       // 操作信息(1字节)
        uint8_t operatorId;                          // 操作员编号(1字节)
        std::array<uint8_t, 6> operationRecordTime;  // 操作记录时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
    };
    std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
};
struct FrameSendData25 : public FrameGB
{                                                            // 8.3.1.11 上传用户信息传输装置软件版本 - typeIdentifier(0x19)
    FrameSendData25(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x19;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
    uint16_t softwareVersion;                                // 软件版本号(2字节，具体格式参考协议定义，如主版本号+次版本号)
};
struct FrameSendData26 : public FrameGB
{                                                            // 8.3.1.12 上传用户信息传输装置配置情况 - typeIdentifier(0x1A)
    FrameSendData26(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x1A;                           // 类型标志符(1字节)
    uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
    uint8_t configDescLength;                                // 配置说明长度(1字节，标识后续配置说明的字节数n)
    std::vector<uint8_t> configDesc;                         // 配置说明(n字节，具体内容由配置说明长度指定)
};
struct FrameSendData28 : public FrameGB
{                                                              // 8.3.1.13 上传用户信息传输装置系统时间 - typeIdentifier(0x1C)
    FrameSendData28(const FrameGB& base) : FrameGB(base) {}    // 用基类对象初始化基类部分(与其他派生类保持一致)
    uint8_t typeIdentifier = 0x1C;                             // 类型标志符(1字节)
    uint8_t objCount;                                          // 信息对象数目(1字节，固定0x01)
    std::array<uint8_t, 6> userInfoTransmissionDeviceSysTime;  // 用户信息传输装置系统时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
};

// 派生类 - 确认 - 对控制命令和发送信息的确认回答 - MSG_SD_YC_CONFIRM
// 派生类 - 否认 - 对控制命令和发送信息的否认回答 - MSG_SD_YC_DENY

class YCParser : public IFrameParser
{
public:
    // 构造/析构函数
    YCParser() = default;
    ~YCParser() = default;

    bool isFrameComplete(const std::vector<uint8_t>& frame) const override
    {
        // 基于帧结构判断完整性：包含完整帧头、帧尾且校验和正确
        if (frame.size() < 2)
            return false;

        // 检查帧头帧尾
        const auto& header = getFrameHeader();
        const auto& tail = getFrameTail();
        if (frame.size() < header.size() + tail.size())
            return false;

        // 检查帧尾
        for (size_t i = 0; i < tail.size(); ++i) {
            if (frame[frame.size() - tail.size() + i] != tail[i]) {
                return false;
            }
        }

        // 最小长度检查
        return frame.size() >= 10;  // 最小帧长度
    }

    bool parse(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg) override;

    bool saveToDB(const FrameBase& frame, const std::string& sessionUuid) override;

    const std::vector<uint8_t>& getFrameHeader() const override
    {
        static const std::vector<uint8_t> header GB_FRAME_HEADER;
        return header;
    }

    const std::vector<uint8_t>& getFrameTail() const override
    {
        static const std::vector<uint8_t> tail GB_FRAME_TAIL;
        return tail;
    }

    std::vector<uint8_t> getCurrentTimeBCD();

private:
    uint8_t bcdToDecimal(uint8_t bcd);
    bool parseSendData01Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData01& result);
    bool parseSendData02Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData02& result);
    bool parseSendData03Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData03& result);
    bool parseSendData04Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData04& result);
    bool parseSendData05Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData05& result);
    bool parseSendData06Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData06& result);
    bool parseSendData07Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData07& result);
    bool parseSendData08Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData08& result);
    bool parseSendData21Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData21& result);
    bool parseSendData24Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData24& result);
    bool parseSendData25Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData25& result);
    bool parseSendData26Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData26& result);
    bool parseSendData28Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData28& result);
};

#endif  // FRAME_SD_YC_PARSER_H