#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "../../../configs/ProtocolConfig.h"
#include "../../../configs/frames/sd/YCConfig.h"
#include "../../../handlers/mysql/MySQLConnectionPool.h"
#include "../../../logs/Logger.h"
#include "../../../utils/UtilsHex.h"

#include "YCParser.h"

bool YCParser::parse(const std::vector<uint8_t>& frame, std::unique_ptr<FrameBase>& outFrame, std::string& errorMsg)
{
    try {
        size_t pos = 0;
        std::unique_ptr<FrameGB> result;

        // 解析公共字段
        auto baseFrame = std::make_unique<FrameGB>();
        baseFrame->protocol = "SD_YC";
        baseFrame->rawData = frame;

        // 1. 帧头(2字节，固定0x40 0x40)- 第1~2字节
        baseFrame->header[0] = frame[pos++];
        baseFrame->header[1] = frame[pos++];
        LOG_DEBUG_FMT("帧头第1字节: 0x%02X", baseFrame->header[0]);
        LOG_DEBUG_FMT("帧头第2字节: 0x%02X", baseFrame->header[1]);

        // 2. 业务流水号(2字节，低字节在前)- 第3~4字节
        baseFrame->serialNumber = frame[pos] | (static_cast<uint16_t>(frame[pos + 1]) << 8);
        pos += 2;
        LOG_DEBUG_FMT("业务流水号(16位): 0x%04X", baseFrame->serialNumber);

        // 3. 协议版本号(2字节)- 第5~6字节
        baseFrame->version[0] = frame[pos++];
        baseFrame->version[1] = frame[pos++];
        LOG_DEBUG_FMT("协议版本号第1字节: 0x%02X", baseFrame->version[0]);
        LOG_DEBUG_FMT("协议版本号第2字节: 0x%02X", baseFrame->version[1]);

        // 4. 时间标签(6字节)- 第7~12字节 [秒,分,时,日,月,年]
        for (int i = 0; i < 6; ++i) {
            baseFrame->timeTag[i] = frame[pos++];
            LOG_DEBUG_FMT("时间标签第%d字节(%s): 0x%02X",
                          i + 1,
                          (i == 0   ? "秒"
                           : i == 1 ? "分"
                           : i == 2 ? "时"
                           : i == 3 ? "日"
                           : i == 4 ? "月"
                                    : "年"),
                          baseFrame->timeTag[i]);
        }

        // 5. 源地址(6字节，低字节在前)- 第13~18字节
        for (int i = 0; i < 6; ++i) {
            baseFrame->sourceAddress[i] = frame[pos++];
        }
        LOG_DEBUG_FMT("源地址 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                      baseFrame->sourceAddress[0],
                      baseFrame->sourceAddress[1],
                      baseFrame->sourceAddress[2],
                      baseFrame->sourceAddress[3],
                      baseFrame->sourceAddress[4],
                      baseFrame->sourceAddress[5]);

        // 6. 目的地址(6字节，低字节在前)- 第19~24字节
        for (int i = 0; i < 6; ++i) {
            baseFrame->targetAddress[i] = frame[pos++];
            LOG_DEBUG_FMT("目的地址第%d字节: 0x%02X", i + 1, baseFrame->targetAddress[i]);
        }

        // 7. 应用数据单元长度(2字节，低字节在前，≤1024)- 第25~26字节
        baseFrame->dataLength = frame[pos] | (static_cast<uint16_t>(frame[pos + 1]) << 8);
        pos += 2;
        LOG_DEBUG_FMT("应用数据单元长度(16位): 0x%04X", baseFrame->dataLength);

        // 8. 命令字节(1字节)- 第27字节
        baseFrame->commandByte = frame[pos++];
        LOG_DEBUG_FMT("命令字节: 0x%02X", baseFrame->commandByte);
        // 新增逻辑：判断 commandByte 是否为 0x02，再解析 typeIdentifier
        if (baseFrame->commandByte != 0x02) {
            errorMsg = "命令字节不是0x02，不支持的类型";
            return false;
        }

        // 9. 类型标识(1字节)- 第28字节
        uint8_t typeId = frame[pos++];
        LOG_DEBUG_FMT("类型标识: 0x%02X", typeId);
        if (typeId == GB_MSG_SEND_DATA_01) {
            auto sendDataFrame01 = std::make_unique<FrameSendData01>(*baseFrame);
            sendDataFrame01->msgId = MSG_SD_YC_SEND_DATA_01;
            sendDataFrame01->typeIdentifier = typeId;
            if (!parseSendData01Frame(frame, pos, *sendDataFrame01)) {
                errorMsg = "上传建筑消防设施系统状态 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame01);
        } else if (typeId == GB_MSG_SEND_DATA_02) {
            auto sendDataFrame02 = std::make_unique<FrameSendData02>(*baseFrame);
            sendDataFrame02->msgId = MSG_SD_YC_SEND_DATA_02;
            sendDataFrame02->typeIdentifier = typeId;
            if (!parseSendData02Frame(frame, pos, *sendDataFrame02)) {
                errorMsg = "上传建筑消防设施部件运行状态 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame02);
        } else if (typeId == GB_MSG_SEND_DATA_03) {
            auto sendDataFrame03 = std::make_unique<FrameSendData03>(*baseFrame);
            sendDataFrame03->msgId = MSG_SD_YC_SEND_DATA_03;
            sendDataFrame03->typeIdentifier = typeId;
            if (!parseSendData03Frame(frame, pos, *sendDataFrame03)) {
                errorMsg = "上传建筑消防设施部件模拟量值 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame03);
        } else if (typeId == GB_MSG_SEND_DATA_04) {
            auto sendDataFrame04 = std::make_unique<FrameSendData04>(*baseFrame);
            sendDataFrame04->msgId = MSG_SD_YC_SEND_DATA_04;
            sendDataFrame04->typeIdentifier = typeId;
            if (!parseSendData04Frame(frame, pos, *sendDataFrame04)) {
                errorMsg = "上传建筑消防设施操作信息记录 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame04);
        } else if (typeId == GB_MSG_SEND_DATA_05) {
            auto sendDataFrame05 = std::make_unique<FrameSendData05>(*baseFrame);
            sendDataFrame05->msgId = MSG_SD_YC_SEND_DATA_05;
            sendDataFrame05->typeIdentifier = typeId;
            if (!parseSendData05Frame(frame, pos, *sendDataFrame05)) {
                errorMsg = "上传建筑消防设施软件版本 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame05);
        } else if (typeId == GB_MSG_SEND_DATA_06) {
            auto sendDataFrame06 = std::make_unique<FrameSendData06>(*baseFrame);
            sendDataFrame06->msgId = MSG_SD_YC_SEND_DATA_06;
            sendDataFrame06->typeIdentifier = typeId;
            if (!parseSendData06Frame(frame, pos, *sendDataFrame06)) {
                errorMsg = "上传建筑消防设施系统配置情况 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame06);
        } else if (typeId == GB_MSG_SEND_DATA_07) {
            auto sendDataFrame07 = std::make_unique<FrameSendData07>(*baseFrame);
            sendDataFrame07->msgId = MSG_SD_YC_SEND_DATA_07;
            sendDataFrame07->typeIdentifier = typeId;
            if (!parseSendData07Frame(frame, pos, *sendDataFrame07)) {
                errorMsg = "上传建筑消防设施部件配置情况 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame07);
        } else if (typeId == GB_MSG_SEND_DATA_08) {
            auto sendDataFrame08 = std::make_unique<FrameSendData08>(*baseFrame);
            sendDataFrame08->msgId = MSG_SD_YC_SEND_DATA_08;
            sendDataFrame08->typeIdentifier = typeId;
            if (!parseSendData08Frame(frame, pos, *sendDataFrame08)) {
                errorMsg = "上传建筑消防设施系统时间 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame08);
        } else if (typeId == GB_MSG_SEND_DATA_21) {
            auto sendDataFrame21 = std::make_unique<FrameSendData21>(*baseFrame);
            sendDataFrame21->msgId = MSG_SD_YC_SEND_DATA_21;
            sendDataFrame21->typeIdentifier = typeId;
            if (!parseSendData21Frame(frame, pos, *sendDataFrame21)) {
                errorMsg = "上传用户信息传输装置运行状态 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame21);
        } else if (typeId == GB_MSG_SEND_DATA_24) {
            auto sendDataFrame24 = std::make_unique<FrameSendData24>(*baseFrame);
            sendDataFrame24->msgId = MSG_SD_YC_SEND_DATA_24;
            sendDataFrame24->typeIdentifier = typeId;
            if (!parseSendData24Frame(frame, pos, *sendDataFrame24)) {
                errorMsg = "上传用户信息传输装置操作信息记录 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame24);
        } else if (typeId == GB_MSG_SEND_DATA_25) {
            auto sendDataFrame25 = std::make_unique<FrameSendData25>(*baseFrame);
            sendDataFrame25->msgId = MSG_SD_YC_SEND_DATA_25;
            sendDataFrame25->typeIdentifier = typeId;
            if (!parseSendData25Frame(frame, pos, *sendDataFrame25)) {
                errorMsg = "上传用户信息传输装置软件版本 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame25);
        } else if (typeId == GB_MSG_SEND_DATA_26) {
            auto sendDataFrame26 = std::make_unique<FrameSendData26>(*baseFrame);
            sendDataFrame26->msgId = MSG_SD_YC_SEND_DATA_26;
            sendDataFrame26->typeIdentifier = typeId;
            if (!parseSendData26Frame(frame, pos, *sendDataFrame26)) {
                errorMsg = "上传用户信息传输装置配置情况 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame26);
        } else if (typeId == GB_MSG_SEND_DATA_28) {
            auto sendDataFrame28 = std::make_unique<FrameSendData28>(*baseFrame);
            sendDataFrame28->msgId = MSG_SD_YC_SEND_DATA_28;
            sendDataFrame28->typeIdentifier = typeId;
            if (!parseSendData28Frame(frame, pos, *sendDataFrame28)) {
                errorMsg = "上传用户信息传输装置系统时间 解析失败";
                return false;
            }
            outFrame = std::move(sendDataFrame28);
        } else {
            errorMsg = "未启用的命令字节";
            return false;
        }

        // 9. 校验和(1字节，倒数第3字节)
        auto& sdFrame = static_cast<FrameGB&>(*outFrame);
        sdFrame.checksum = frame[pos++];
        LOG_DEBUG_FMT("校验和: 0x%02X", sdFrame.checksum);
        uint8_t calculatedChecksum = 0;
        for (size_t i = 2; i < frame.size() - 3; ++i) {
            calculatedChecksum += frame[i];
            LOG_DEBUG_FMT("累加 0x%02X, 和：%hu", frame[i], calculatedChecksum);
        }
        calculatedChecksum &= 0xFF;  // 取低8位作为最终校验和
        if (calculatedChecksum != sdFrame.checksum) {
            LOG_DEBUG_FMT(
                "校验和不匹配，计算值: 0x%02X (%hhu), 实际值: 0x%02X (%hhu)", calculatedChecksum, calculatedChecksum, sdFrame.checksum, sdFrame.checksum);
            errorMsg = "校验和不匹配";
            return false;
        } else {
            LOG_DEBUG("校验和通过");
        }

        // 10. 帧尾(2字节，固定0x23 0x23)- 最后2字节
        sdFrame.tail[0] = frame[pos++];
        sdFrame.tail[1] = frame[pos++];
        LOG_DEBUG_FMT("帧尾第1字节: 0x%02X", sdFrame.tail[0]);
        LOG_DEBUG_FMT("帧尾第2字节: 0x%02X", sdFrame.tail[1]);

        return true;
    } catch (const std::exception& e) {
        errorMsg = "解析异常: " + std::string(e.what());
        return false;
    }
}

// struct FrameSendData01 : public FrameGB {                  // 8.3.1.1 上传建筑消防设施系统状态 - typeIdentifier(0x01)
//   FrameSendData01(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于102)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t systemType;                                    // 系统类型(1字节)
//     uint8_t systemAddress;                                 // 系统地址(1字节)
//     uint16_t systemStatus;                                 // 系统状态(2字节)
//     std::array<uint8_t, 6> statuOccurrenceTime;            // 状态发生时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData01Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData01& result)
{
    LOG_INFO("01 - 上传建筑消防设施系统状态");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 102)
        LOG_ERROR("信息对象数目错误,大于102");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData01::InfoObject infoObj;
        infoObj.systemType = frame[pos++];
        LOG_DEBUG_FMT("信息对象[%d]系统类型: 0x%02X", i, infoObj.systemType);
        infoObj.systemAddress = frame[pos++];
        LOG_DEBUG_FMT("信息对象[%d]系统地址: 0x%02X", i, infoObj.systemAddress);
        infoObj.systemStatus = (static_cast<uint16_t>(frame[pos])) | (static_cast<uint16_t>(frame[pos + 1]) << 8);
        pos += 2;
        LOG_DEBUG_FMT("信息对象[%d]系统状态: 0x%04X", i, infoObj.systemStatus);
        for (int j = 0; j < 6; ++j) {
            infoObj.statuOccurrenceTime[j] = frame[pos++];
        }
        LOG_DEBUG_FMT("状态发生时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                      infoObj.statuOccurrenceTime[0],
                      infoObj.statuOccurrenceTime[1],
                      infoObj.statuOccurrenceTime[2],
                      infoObj.statuOccurrenceTime[3],
                      infoObj.statuOccurrenceTime[4],
                      infoObj.statuOccurrenceTime[5]);

        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData02 : public FrameGB {                  // 8.3.1.2 上传建筑消防设施部件运行状态 - typeIdentifier(0x02)
//   FrameSendData02(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于22)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t systemType;                                    // 系统类型(1字节)
//     uint8_t systemAddress;                                 // 系统地址(1字节)
//     uint8_t componentType;                                 // 部件类型(1字节)
//     uint32_t componentAddress;                             // 部件地址(4字节)
//     uint16_t componentStatus;                              // 部件状态(2字节)
//     std::array<uint8_t, 31> componentDesc;                 // 部件说明(31字节)
//     std::array<uint8_t, 6> statuOccurrenceTime;            // 状态发生时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData02Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData02& result)
{
    LOG_INFO("02 - 上传建筑消防设施部件运行状态");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 22)
        LOG_ERROR("信息对象数目错误,大于22");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData02::InfoObject infoObj;
        infoObj.systemType = frame[pos++];
        infoObj.systemAddress = frame[pos++];
        infoObj.componentType = frame[pos++];
        infoObj.componentAddress = (static_cast<uint32_t>(frame[pos])) | (static_cast<uint32_t>(frame[pos + 1]) << 8) |
                                   (static_cast<uint32_t>(frame[pos + 2]) << 16) | (static_cast<uint32_t>(frame[pos + 3]) << 24);
        pos += 4;
        infoObj.componentStatus = (static_cast<uint16_t>(frame[pos])) | (static_cast<uint16_t>(frame[pos + 1]) << 8);
        pos += 2;
        for (int j = 0; j < 31; ++j) {
            infoObj.componentDesc[j] = frame[pos++];
        }
        for (int j = 0; j < 6; ++j) {
            infoObj.statuOccurrenceTime[j] = frame[pos++];
        }
        LOG_DEBUG_FMT("状态发生时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                      infoObj.statuOccurrenceTime[0],
                      infoObj.statuOccurrenceTime[1],
                      infoObj.statuOccurrenceTime[2],
                      infoObj.statuOccurrenceTime[3],
                      infoObj.statuOccurrenceTime[4],
                      infoObj.statuOccurrenceTime[5]);

        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData03 : public FrameGB {                  // 8.3.1.3 上传建筑消防设施部件模拟量值 - typeIdentifier(0x03)
//   FrameSendData03(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于63)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t systemType;                                    // 系统类型(1字节)
//     uint8_t systemAddress;                                 // 系统地址(1字节)
//     uint8_t componentType;                                 // 部件类型(1字节)
//     uint32_t componentAddress;                             // 部件地址(4字节)
//     uint8_t analogType;                                    // 模拟量类型(1字节)
//     uint16_t analogValue;                                  // 模拟量值(2字节)
//     std::array<uint8_t, 6> analogValueSampleTime;          // 模拟量值采样时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData03Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData03& result)
{
    LOG_INFO("03 - 上传建筑消防设施部件模拟量值");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 63)
        LOG_ERROR("信息对象数目错误,大于63");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData03::InfoObject infoObj;
        infoObj.systemType = frame[pos++];
        infoObj.systemAddress = frame[pos++];
        infoObj.componentType = frame[pos++];
        infoObj.componentAddress = (static_cast<uint32_t>(frame[pos])) | (static_cast<uint32_t>(frame[pos + 1]) << 8) |
                                   (static_cast<uint32_t>(frame[pos + 2]) << 16) | (static_cast<uint32_t>(frame[pos + 3]) << 24);
        infoObj.analogType = frame[pos++];
        infoObj.analogValue = (static_cast<uint16_t>(frame[pos])) | (static_cast<uint16_t>(frame[pos + 1]) << 8);
        for (int j = 0; j < 6; ++j) {
            infoObj.analogValueSampleTime[j] = frame[pos++];
        }
        LOG_DEBUG_FMT("模拟量值采样时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                      infoObj.analogValueSampleTime[0],
                      infoObj.analogValueSampleTime[1],
                      infoObj.analogValueSampleTime[2],
                      infoObj.analogValueSampleTime[3],
                      infoObj.analogValueSampleTime[4],
                      infoObj.analogValueSampleTime[5]);

        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData04 : public FrameGB {                  // 8.3.1.4 上传建筑消防设施操作信息记录 - typeIdentifier(0x04)
//   FrameSendData04(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于102)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t systemType;                                    // 系统类型(1字节)
//     uint8_t systemAddress;                                 // 系统地址(1字节)
//     uint8_t operationInfo;                                 // 操作信息(1字节)
//     uint8_t operatorId;                                    // 操作员编号(1字节)
//     std::array<uint8_t, 6> operationRecordTime;            // 操作记录时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData04Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData04& result)
{
    LOG_INFO("04 - 上传建筑消防设施操作信息记录");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 102)
        LOG_ERROR("信息对象数目错误,大于102");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData04::InfoObject infoObj;
        infoObj.systemType = frame[pos++];
        infoObj.systemAddress = frame[pos++];
        infoObj.operationInfo = frame[pos++];
        infoObj.operatorId = frame[pos++];
        for (int j = 0; j < 6; ++j) {
            infoObj.operationRecordTime[j] = frame[pos++];
        }
        LOG_DEBUG_FMT("操作记录时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                      infoObj.operationRecordTime[0],
                      infoObj.operationRecordTime[1],
                      infoObj.operationRecordTime[2],
                      infoObj.operationRecordTime[3],
                      infoObj.operationRecordTime[4],
                      infoObj.operationRecordTime[5]);

        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData05 : public FrameGB {                  // 8.3.1.5 上传建筑消防设施软件版本 - typeIdentifier(0x05)
//   FrameSendData05(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
//   uint8_t systemType;                                      // 系统类型(1字节)
//   uint8_t systemAddress;                                   // 系统地址(1字节)
//   uint8_t softwareMajorVersion;                            // 软件主版本号(1字节)
//   uint8_t softwareMinorVersion;                            // 软件次版本号(1字节)
// };
bool YCParser::parseSendData05Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData05& result)
{
    LOG_INFO("05 - 上传建筑消防设施软件版本");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 1)
        LOG_ERROR("信息对象数目错误,大于1");

    result.systemType = frame[pos++];
    result.systemAddress = frame[pos++];
    result.softwareMajorVersion = frame[pos++];
    result.softwareMinorVersion = frame[pos++];
    return true;
}

// struct FrameSendData06 : public FrameGB {                  // 8.3.1.6 上传建筑消防设施系统配置情况 - typeIdentifier(0x06)
//   FrameSendData06(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于3)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t systemType;                                    // 系统类型(1字节)
//     uint8_t systemAddress;                                 // 系统地址(1字节)
//     uint8_t systemDescLength;                              // 系统说明长度(1字节，n为后续系统说明的字节数)
//     std::vector<uint8_t> systemDesc;                       // 系统说明(n字节，长度由systemDescLength指定)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData06Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData06& result)
{
    LOG_INFO("06 - 上传建筑消防设施系统配置情况");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 3)
        LOG_ERROR("信息对象数目错误,大于3");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData06::InfoObject infoObj;
        infoObj.systemType = frame[pos++];
        infoObj.systemAddress = frame[pos++];
        infoObj.systemDescLength = frame[pos++];
        for (uint8_t j = 0; j < infoObj.systemDescLength; ++j) {
            infoObj.systemDesc[j] = frame[pos++];
        }

        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData07 : public FrameGB {                  // 8.3.1.7 上传建筑消防设施部件配置情况 - typeIdentifier(0x07)
//   FrameSendData07(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于26)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t systemType;                                    // 系统类型(1字节)
//     uint8_t systemAddress;                                 // 系统地址(1字节)
//     uint8_t componentType;                                 // 部件类型(1字节)
//     uint32_t componentAddress;                             // 部件地址(4字节)
//     std::array<uint8_t, 31> componentDesc;                 // 部件说明(31字节，固定长度)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData07Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData07& result)
{
    LOG_INFO("07 - 上传建筑消防设施部件配置情况");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 26)
        LOG_ERROR("信息对象数目错误,大于26");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData07::InfoObject infoObj;
        infoObj.systemType = frame[pos++];
        infoObj.systemAddress = frame[pos++];
        infoObj.componentType = frame[pos++];
        infoObj.componentAddress = (static_cast<uint32_t>(frame[pos])) | (static_cast<uint32_t>(frame[pos + 1]) << 8) |
                                   (static_cast<uint32_t>(frame[pos + 2]) << 16) | (static_cast<uint32_t>(frame[pos + 3]) << 24);
        for (int j = 0; j < 31; ++j) {
            infoObj.componentDesc[j] = frame[pos++];
        }

        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData08 : public FrameGB {                  // 8.3.1.8 上传建筑消防设施系统时间 - typeIdentifier(0x08)
//   FrameSendData08(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
//   uint8_t systemType;                                      // 系统类型(1字节)
//   uint8_t systemAddress;                                   // 系统地址(1字节)
//   std::array<uint8_t, 6> buildingFireFacilitySysTime;      // 建筑消防设施的系统时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
// };
bool YCParser::parseSendData08Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData08& result)
{
    LOG_INFO("08 - 上传建筑消防设施系统时间");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 1)
        LOG_ERROR("信息对象数目错误,大于1");

    result.systemType = frame[pos++];
    result.systemAddress = frame[pos++];
    for (int j = 0; j < 6; ++j) {
        result.buildingFireFacilitySysTime[j] = frame[pos++];
    }
    LOG_DEBUG_FMT("建筑消防设施的系统时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                  result.buildingFireFacilitySysTime[0],
                  result.buildingFireFacilitySysTime[1],
                  result.buildingFireFacilitySysTime[2],
                  result.buildingFireFacilitySysTime[3],
                  result.buildingFireFacilitySysTime[4],
                  result.buildingFireFacilitySysTime[5]);
    return true;
}

// struct FrameSendData21 : public FrameGB {                  // 8.3.1.9 上传用户信息传输装置运行状态 - typeIdentifier(0x15)
//   FrameSendData21(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
//   uint8_t state;                                           // 状态(1字节，具体状态值需参考协议定义)
//   std::array<uint8_t, 6> statuOccurrenceTime;              // 建筑消防设施的系统时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
// };
bool YCParser::parseSendData21Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData21& result)
{
    LOG_INFO("21 - 上传用户信息传输装置运行状态");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 1)
        LOG_ERROR("信息对象数目错误,大于1");

    result.state = frame[pos++];
    LOG_DEBUG_FMT("状态: 0x%02X", result.state);
    for (int j = 0; j < 6; ++j) {
        result.statuOccurrenceTime[j] = frame[pos++];
    }
    LOG_DEBUG_FMT("建筑消防设施的系统时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                  result.statuOccurrenceTime[0],
                  result.statuOccurrenceTime[1],
                  result.statuOccurrenceTime[2],
                  result.statuOccurrenceTime[3],
                  result.statuOccurrenceTime[4],
                  result.statuOccurrenceTime[5]);
    return true;
}

// struct FrameSendData24 : public FrameGB {                  // 8.3.1.10 上传用户信息传输装置操作信息记录 - typeIdentifier(0x18)
//   FrameSendData24(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，等于几(n不大于127)，后面就有几个形同结构的结构体
//   struct InfoObject {                                      // 信息对象结构体(根据objCount的值存在多个)
//     uint8_t operationInfo;                                 // 操作信息(1字节)
//     uint8_t operatorId;                                    // 操作员编号(1字节)
//     std::array<uint8_t, 6> operationRecordTime;            // 操作记录时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
//   };
//   std::vector<InfoObject> infoObjects;  // 信息对象列表(数量由objCount指定)
// };
bool YCParser::parseSendData24Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData24& result)
{
    LOG_INFO("24 - 上传用户信息传输装置操作信息记录");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 127)
        LOG_ERROR("信息对象数目错误,大于127");

    // InfoObject 信息对象结构体(根据objCount的值存在多个)
    for (uint8_t i = 0; i < result.objCount; ++i) {
        FrameSendData24::InfoObject infoObj;
        infoObj.operationInfo = frame[pos++];
        LOG_DEBUG_FMT("操作信息: 0x%02X", infoObj.operationInfo);
        infoObj.operatorId = frame[pos++];
        LOG_DEBUG_FMT("操作员编号: 0x%02X", infoObj.operatorId);
        for (int j = 0; j < 6; ++j) {
            infoObj.operationRecordTime[j] = frame[pos++];
        }
        LOG_DEBUG_FMT("操作记录时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
                      infoObj.operationRecordTime[0],
                      infoObj.operationRecordTime[1],
                      infoObj.operationRecordTime[2],
                      infoObj.operationRecordTime[3],
                      infoObj.operationRecordTime[4],
                      infoObj.operationRecordTime[5]);
        // 将解析好的信息对象添加到列表
        result.infoObjects.push_back(infoObj);
        LOG_DEBUG_FMT("已解析第%d个InfoObject", i + 1);
    }
    return true;
}

// struct FrameSendData25 : public FrameGB {                  // 8.3.1.11 上传用户信息传输装置软件版本 - typeIdentifier(0x19)
//   FrameSendData25(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
//   uint16_t softwareVersion;                                // 软件版本号(2字节，具体格式参考协议定义，如主版本号+次版本号)
// };
bool YCParser::parseSendData25Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData25& result)
{
    LOG_INFO("25 - 上传用户信息传输装置软件版本");

    result.objCount = frame[pos++];  // 解析信息对象数目
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 1)
        LOG_ERROR("信息对象数目错误,大于1");

    result.softwareVersion =
        (static_cast<uint16_t>(frame[pos])) | (static_cast<uint16_t>(frame[pos + 1]) << 8);  // 软件版本号(2字节，具体格式参考协议定义，如主版本号+次版本号)

    return true;
}

// struct FrameSendData26 : public FrameGB {                  // 8.3.1.12 上传用户信息传输装置配置情况 - typeIdentifier(0x1A)
//   FrameSendData26(const FrameGB& base) : FrameGB(base) {}  // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                        // 信息对象数目(1字节)，固定1
//   uint8_t configDescLength;                                // 配置说明长度(1字节，标识后续配置说明的字节数n)
//   std::vector<uint8_t> configDesc;                         // 配置说明(n字节，具体内容由配置说明长度指定)
// };
bool YCParser::parseSendData26Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData26& result)
{
    LOG_INFO("26 - 上传用户信息传输装置配置情况");

    result.objCount = frame[pos++];
    LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 1)
        LOG_ERROR("信息对象数目错误,大于1");

    result.configDescLength = frame[pos++];
    for (uint8_t j = 0; j < result.configDescLength; ++j) {
        result.configDesc[j] = frame[pos++];
    }

    return true;
}

// struct FrameSendData28 : public FrameGB {                    // 8.3.1.13 上传用户信息传输装置系统时间 - typeIdentifier(0x1C)
//   FrameSendData28(const FrameGB& base) : FrameGB(base) {}    // 用基类对象初始化基类部分(与其他派生类保持一致)
//   uint8_t objCount;                                          // 信息对象数目(1字节，固定0x01)
//   std::array<uint8_t, 6> userInfoTransmissionDeviceSysTime;  // 用户信息传输装置系统时间(6字节，格式：[0]秒, [1]分, [2]时, [3]日, [4]月, [5]年)
// };
bool YCParser::parseSendData28Frame(const std::vector<uint8_t>& frame, size_t& pos, FrameSendData28& result)
{
    LOG_INFO("28 - 上传用户信息传输装置系统时间");
    result.objCount = frame[pos++];
    // LOG_DEBUG_FMT("信息对象数目: 0x%02X", result.objCount);
    if (result.objCount > 1)
        LOG_ERROR("信息对象数目错误,大于1");

    for (int j = 0; j < 6; ++j) {
        result.userInfoTransmissionDeviceSysTime[j] = frame[pos++];
    }
    // LOG_DEBUG_FMT("用户信息传输装置系统时间: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", result.userInfoTransmissionDeviceSysTime[0],
    // result.userInfoTransmissionDeviceSysTime[1],
    //               result.userInfoTransmissionDeviceSysTime[2], result.userInfoTransmissionDeviceSysTime[3], result.userInfoTransmissionDeviceSysTime[4],
    //               result.userInfoTransmissionDeviceSysTime[5]);
    return true;
}

bool YCParser::saveToDB(const FrameBase& frame, const std::string& sessionUuid)
{
    const auto& sdFrame = static_cast<const FrameGB&>(frame);
    auto mysqlPool = MySQLConnectionPool::ConnectionGuard(MySQLConnectionPool::getInstance().getConnection());
    if (!mysqlPool.get()) {
        LOG_ERROR("获取MySQL连接失败");
        return false;
    }
    std::map<std::string, std::string> frameData;
    // 根据来源地址，解析出设备sn
    uint8_t tempSourceAddress[6];
    for (int i = 0; i < 6; ++i) {
        tempSourceAddress[i] = sdFrame.sourceAddress[5 - i];
    }
    uint64_t deviceIOTSn = 0;
    for (int i = 0; i < 6; ++i) {
        deviceIOTSn = (deviceIOTSn << 8) | tempSourceAddress[i];  // 大端序按字节从高到低拼接（左移8位腾出空间，再拼接当前字节）
    }
    frameData["device_number"] = std::to_string(deviceIOTSn);
    frameData["frame_raw"] = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(sdFrame.rawData.data()), sdFrame.rawData.size());
    frameData["protocal_type"] = "SD_YC";
    if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_01) {
        frameData["frame_type"] = "send_data_up_01";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_02) {
        frameData["frame_type"] = "send_data_up_02";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_03) {
        frameData["frame_type"] = "send_data_up_03";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_04) {
        frameData["frame_type"] = "send_data_up_04";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_05) {
        frameData["frame_type"] = "send_data_up_05";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_06) {
        frameData["frame_type"] = "send_data_up_06";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_07) {
        frameData["frame_type"] = "send_data_up_07";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_08) {
        frameData["frame_type"] = "send_data_up_08";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_21) {
        frameData["frame_type"] = "send_data_up_21";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_24) {
        frameData["frame_type"] = "send_data_up_24";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_25) {
        frameData["frame_type"] = "send_data_up_25";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_26) {
        frameData["frame_type"] = "send_data_up_26";
    } else if (sdFrame.msgId == MSG_SD_YC_SEND_DATA_28) {
        frameData["frame_type"] = "send_data_up_28";
    }
    frameData["data_length"] = std::to_string(sdFrame.dataLength);
    frameData["session_uuid"] = sessionUuid;
    frameData["serial_number"] = std::to_string(sdFrame.serialNumber);
    frameData["source_address"] = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(sdFrame.sourceAddress.data()), sdFrame.sourceAddress.size());

    return mysqlPool->insertOne("pool_iot_devices_data_frame_raw", frameData);
}

uint8_t YCParser::bcdToDecimal(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

// 新增：获取当前时间的BCD码
std::vector<uint8_t> YCParser::getCurrentTimeBCD()
{
    std::vector<uint8_t> timeBCD(7);  // 7字节：年高、年低、月、日、时、分、秒

    // 获取当前系统时间
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_r(&time, &localTime);  // 转换为本地时间

    // 转换为BCD码(注意：BCD码每个十进制数字用4位二进制表示)
    int year = localTime.tm_year + 1900;  // tm_year是从1900年开始的偏移量
    timeBCD[0] = (year / 100) % 100;      // 年高位(如2024年 → 20 → 0x20)
    timeBCD[1] = year % 100;              // 年低位(如2024年 → 24 → 0x24)
    timeBCD[2] = localTime.tm_mon + 1;    // 月(tm_mon从0开始，+1)
    timeBCD[3] = localTime.tm_mday;       // 日
    timeBCD[4] = localTime.tm_hour;       // 时
    timeBCD[5] = localTime.tm_min;        // 分
    timeBCD[6] = localTime.tm_sec;        // 秒

    // 将每个值转换为BCD码(如 12 → 0x12)
    for (auto& val : timeBCD) {
        val = ((val / 10) << 4) | (val % 10);
    }

    return timeBCD;
}
