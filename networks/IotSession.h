#pragma once
#ifndef IOT_SESSION_H
#define IOT_SESSION_H

#include <boost/asio.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <mutex>
#include <queue>

#include "../configs/frames/hy/LoRaConfig.h"
#include "../configs/frames/sd/RTUConfig.h"
#include "../frames/FrameAssembler.h"
#include "../frames/FrameAssemblerFactory.h"
#include "../frames/FrameParserDispatcher.h"
#include "../frames/IFrameParser.h"
#include "../frames/hy/LoRaParser.h"
#include "../frames/sd/rtu/RTUParser.h"
#include "../nodes/MsgNodeRecv.h"
#include "../nodes/MsgNodeSend.h"

class CServer;
class LogicSystem;

// 会话类：管理单个客户端TCP连接，负责数据的读取、发送和解析
class IotSession : public std::enable_shared_from_this<IotSession>
{
public:
    IotSession(boost::asio::io_context& io_context, CServer* server);  // 构造函数：初始化会话
    ~IotSession();                                                     // 析构函数：释放会话资源
    boost::asio::ip::tcp::socket& getSocket();                         // 获取会话的socket（供服务器accept使用）
    std::string& getUuid();                                            // 获取会话的唯一标识（UUID）
    void start();                                                      // 启动会话（开始异步读取客户端数据）
    void send(char* msg, short max_length, int msgid);                 // 发送消息（字符串版本）
    void send(std::string msg, int msgid);                             // 发送消息（字符数组版本）
    void close();                                                      // 关闭会话（关闭socket并标记状态）
    std::shared_ptr<IotSession> sharedSelf();                          // 获取当前会话的shared_ptr（用于异步回调中延长生命周期）
    std::string getCurrentTime();                                      // 格式化时间
    void startRead();

private:
    std::unique_ptr<IFrameAssembler> m_frameAssembler;  // 当前协议组装器
    std::unique_ptr<IFrameParser> m_frameParser;        // 当前协议解析器
    std::string m_currentProtocol;                      // 当前协议名（如"SD_RTU"、"GB/T26875.3"）
    std::vector<uint8_t> m_recvBuffer;                  // 临时缓存未识别协议的字节流
    // void handleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<IotSession> shared_self);  // 处理读取数据的回调函数
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error, std::shared_ptr<IotSession> shared_self);  // 处理发送数据的回调函数
    boost::asio::ip::tcp::socket m_iotSession_socket;                                                   // 与客户端通信的socket
    std::string m_iotSession_UUID;                                                                      // 会话唯一标识（UUID）
    char m_iotSession_data[MAX_LENGTH];                                                                 // 读取数据的缓冲区（大小为MAX_LENGTH）
    CServer* m_iotSession_server;                                                                       // 所属服务器实例（用于通知服务器移除会话）
    bool m_iotSession_bClose;                                                                           // 会话是否已关闭的标志
    std::queue<std::shared_ptr<MsgNodeSend>> m_iotSession_sendQue;                                      // 发送队列（存储待发送的消息）
    std::mutex m_iotSession_sendLock;                                                                   // 发送队列的互斥锁（确保线程安全）
    std::shared_ptr<MsgNodeRecv> m_iotSession_recvMsgNode;                                              // 当前正在接收的消息节点（数据部分）
    std::shared_ptr<MsgNodeRecv> m_iotSession_recvHeadNode;                                             // 用于接收消息头部的节点（4字节）
    void handleParsedFrame(const std::unique_ptr<FrameBase>& frame);
    void handleRTUReportDataValues(const FrameRTUReportData& dataFrame);
};

// 逻辑节点：封装会话和接收的消息，用于提交给业务逻辑系统处理
class LogicNode
{
    // 允许LogicSystem访问私有成员
    friend class LogicSystem;

public:
    // 构造函数：绑定会话和接收的消息
    LogicNode(std::shared_ptr<IotSession>, std::shared_ptr<MsgNodeRecv>);

private:
    // 消息所属的会话
    std::shared_ptr<IotSession> m_logicNode_session;

    // 接收的消息内容
    std::shared_ptr<MsgNodeRecv> m_logicNode_recvnode;
};

#endif  // IOT_SESSION_H