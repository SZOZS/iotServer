#include <cinttypes>
#include <iostream>
#include <sstream>

#include "../frames/FrameAssembler.h"
#include "../frames/FrameAssemblerFactory.h"
#include "../frames/FrameParserDispatcher.h"
#include "../frames/IFrameParser.h"
#include "../frames/hy/LoRaAssembler.h"
#include "../frames/sd/rtu/RTUAssembler.h"
#include "../frames/sd/yc/YCAssembler.h"
#include "../handlers/mysql/MySQLConnectionPool.h"
#include "../logics/LogicSystem.h"
#include "../logs/Logger.h"
#include "../utils/UtilsHex.h"

#include "CServer.h"
#include "IotSession.h"

// 构造函数：初始化会话，生成UUID，准备接收头部的缓冲区
IotSession::IotSession(boost::asio::io_context& io_context, CServer* server)
    : m_iotSession_socket(io_context), m_iotSession_server(server), m_iotSession_bClose(false)
{
    // 生成UUID作为会话唯一标识
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    m_iotSession_UUID = boost::uuids::to_string(a_uuid);
    // 初始化头部接收节点（大小为HEAD_TOTAL_LEN=4字节）
    m_iotSession_recvHeadNode = std::make_shared<MsgNodeRecv>(HEAD_TOTAL_LEN, 0, m_iotSession_UUID);

    LOG_INFO_FMT("[%s]初始化", m_iotSession_UUID.c_str());
}

// 析构函数：输出会话销毁信息
IotSession::~IotSession()
{
    LOG_WARNING_FMT("[%s]已销毁", m_iotSession_UUID.c_str());
}

// 获取会话的socket
boost::asio::ip::tcp::socket& IotSession::getSocket()
{
    return m_iotSession_socket;
}

// 获取会话的UUID
std::string& IotSession::getUuid()
{
    return m_iotSession_UUID;
}

// 启动会话：开始异步读取客户端数据
void IotSession::start()
{
    LOG_INFO_FMT("[%s]接收数据...", m_iotSession_UUID.c_str());

    // 清空接收缓冲区
    ::memset(m_iotSession_data, 0, MAX_LENGTH);
    // 异步读取数据：当有数据到达时，调用handleRead处理
    m_iotSession_socket.async_read_some(boost::asio::buffer(m_iotSession_data, MAX_LENGTH),
                                        std::bind(&IotSession::handleRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

// 发送字符串消息
void IotSession::send(std::string msg, int msgid)
{
    LOG_INFO_FMT("[%s]发送数据...", m_iotSession_UUID.c_str());
    // 加锁保护发送队列
    std::lock_guard<std::mutex> lock(m_iotSession_sendLock);
    size_t send_que_size = m_iotSession_sendQue.size();
    // 若发送队列已满，拒绝发送（防止内存溢出）
    if (send_que_size > MAX_SEND_QUEUE) {
        LOG_ERROR_FMT("[%s]发送失败，超过队列大小[%d]", m_iotSession_UUID.c_str(), MAX_SEND_QUEUE);
        return;
    }

    // 打印待发送数据（转换为十六进制格式，便于查看二进制内容）
    std::string sendHex = utils::UtilsHex::bytesToHexString(msg.c_str(), msg.length());
    LOG_INFO_FMT("[%s]待发送数据（msgid: %d, 长度: %zu字节）: %s", m_iotSession_UUID.c_str(), msgid, msg.length(), sendHex.c_str());

    // 将消息封装为SendNode并加入发送队列
    m_iotSession_sendQue.push(std::make_shared<MsgNodeSend>(msg.c_str(), msg.length(), msgid, m_iotSession_UUID));
    LOG_INFO_FMT("[%s]消息已排队", m_iotSession_UUID.c_str());
    // 若队列之前为空，立即启动异步发送（否则等待前一条消息发送完成后继续）
    if (send_que_size > 0) {
        LOG_WARNING_FMT("[%s]等待发送上一条消息...", m_iotSession_UUID.c_str());
        return;
    }
    auto& msgnode = m_iotSession_sendQue.front();
    boost::asio::async_write(m_iotSession_socket,
                             boost::asio::buffer(msgnode->m_msgNodeSend_data, msgnode->m_msgNodeSend_totalLen),
                             std::bind(&IotSession::handleWrite, this, std::placeholders::_1, sharedSelf()));
}

// 发送字符数组消息
void IotSession::send(char* msg, short max_length, int msgid)
{
    LOG_INFO_FMT("[%s]发送数据...", m_iotSession_UUID.c_str());
    // 加锁保护发送队列
    std::lock_guard<std::mutex> lock(m_iotSession_sendLock);
    size_t send_que_size = m_iotSession_sendQue.size();
    // 若发送队列已满，拒绝发送
    if (send_que_size > MAX_SEND_QUEUE) {
        LOG_ERROR_FMT("[%s]发送失败，超过队列大小[%d]", m_iotSession_UUID.c_str(), MAX_SEND_QUEUE);
        return;
    }
    // 将消息封装为SendNode并加入发送队列
    m_iotSession_sendQue.push(std::make_shared<MsgNodeSend>(msg, static_cast<size_t>(max_length), msgid, m_iotSession_UUID));
    LOG_INFO_FMT("[%s]消息已排队", m_iotSession_UUID.c_str());
    // 若队列之前为空，立即启动异步发送
    if (send_que_size > 0) {
        LOG_WARNING_FMT("[%s]等待发送上一条消息...", m_iotSession_UUID.c_str());
        return;
    }
    auto& msgnode = m_iotSession_sendQue.front();
    boost::asio::async_write(m_iotSession_socket,
                             boost::asio::buffer(msgnode->m_msgNodeSend_data, msgnode->m_msgNodeSend_totalLen),
                             std::bind(&IotSession::handleWrite, this, std::placeholders::_1, sharedSelf()));
}

// 关闭会话：关闭socket并标记状态
void IotSession::close()
{
    LOG_WARNING_FMT("[%s]开始关闭...", m_iotSession_UUID.c_str());
    if (m_iotSession_bClose)
        return;  // 避免重复关闭
    m_iotSession_bClose = true;

    // 显示客户端断开连接信息
    try {
        boost::asio::ip::tcp::endpoint remote_ep = m_iotSession_socket.remote_endpoint();
        std::string client_ip = remote_ep.address().to_string();
        unsigned short client_port = remote_ep.port();

        LOG_WARNING_FMT("[%s]%s:%hu客户端断开连接", m_iotSession_UUID.c_str(), client_ip.c_str(), client_port);
    } catch (const boost::system::system_error& e) {
        LOG_ERROR_FMT("[%s]关闭失败，出现以下错误:%s", m_iotSession_UUID.c_str(), e.what());
    }

    m_iotSession_socket.close();
}

// 获取当前会话的shared_ptr（用于异步回调中保持对象存活）
std::shared_ptr<IotSession> IotSession::sharedSelf()
{
    return shared_from_this();
}

// 处理发送完成的回调
void IotSession::handleWrite(const boost::system::error_code& error, std::shared_ptr<IotSession> shared_self)
{
    LOG_INFO_FMT("[%s]写回调", m_iotSession_UUID.c_str());
    try {
        // 发送成功
        if (!error) {
            std::lock_guard<std::mutex> lock(m_iotSession_sendLock);
            // 移除已发送的消息
            m_iotSession_sendQue.pop();
            // 若队列中还有消息，继续发送下一条
            if (!m_iotSession_sendQue.empty()) {
                auto& msgnode = m_iotSession_sendQue.front();
                boost::asio::async_write(m_iotSession_socket,
                                         boost::asio::buffer(msgnode->m_msgNodeSend_data, msgnode->m_msgNodeSend_totalLen),
                                         std::bind(&IotSession::handleWrite, this, std::placeholders::_1, shared_self));
            }
        } else {  // 发送失败（如客户端断开）
            LOG_ERROR_FMT("[%s]写入回调失败，出现以下错误:%s", m_iotSession_UUID.c_str(), error.message().c_str());
            // 关闭会话
            close();
            // 通知服务器移除会话
            m_iotSession_server->clearIotSession(m_iotSession_UUID);
        }
    } catch (std::exception& e) {
        LOG_ERROR_FMT("[%s]写入回调失败，返回异常代码:%s", m_iotSession_UUID.c_str(), e.what());
    }
}

// void IotSession::handleRead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<IotSession>) {
void IotSession::handleRead(const boost::system::error_code& error, size_t bytes_transferred)
{
    LOG_INFO_FMT("[%s]读回调", m_iotSession_UUID.c_str());

    try {
        // 处理客户端断开相关错误
        if (error && (error == boost::asio::error::eof || error == boost::asio::error::connection_reset || error == boost::asio::error::broken_pipe)) {
            close();
            m_iotSession_server->clearIotSession(m_iotSession_UUID);
            return;
        }
        // 缓存接收的字节流
        m_recvBuffer.insert(m_recvBuffer.end(), m_iotSession_data, m_iotSession_data + bytes_transferred);
        // 打印当前m_recvBuffer的内容（十六进制格式）
        std::string recvHex = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(m_recvBuffer.data()), m_recvBuffer.size());
        LOG_INFO_FMT("[%s]接收缓冲区更新后内容 (长度: %zu 字节): %s", m_iotSession_UUID.c_str(), m_recvBuffer.size(), recvHex.c_str());
        // 未识别协议时，尝试识别
        if (m_currentProtocol.empty()) {
            LOG_INFO_FMT("[%s]识别协议 开始", m_iotSession_UUID.c_str());
            std::string protocol = FrameParserDispatcher::identifyProtocol(m_recvBuffer);
            if (!protocol.empty()) {
                // 绑定协议对应的解析器和组装器
                m_currentProtocol = protocol;
                m_frameParser = FrameParserFactory::createParser(protocol);
                m_frameAssembler = FrameAssemblerFactory::createAssembler(protocol);
                LOG_INFO_FMT("[%s]识别协议:%s", m_iotSession_UUID.c_str(), protocol.c_str());
            } else if (m_recvBuffer.size() > MAX_FRAME_LEN) {
                // 超过最大帧长仍未识别，重置缓存（避免内存溢出）
                m_recvBuffer.clear();
                LOG_WARNING_FMT("[%s]未识别协议，重置缓存", m_iotSession_UUID.c_str());
            }
        }

        // 已识别协议，使用对应组装器处理字节流
        if (!m_currentProtocol.empty() && m_frameAssembler) {
            LOG_INFO_FMT("[%s]帧解析 开始", m_iotSession_UUID.c_str());
            for (uint8_t byte : std::vector<uint8_t>(m_iotSession_data, m_iotSession_data + bytes_transferred)) {
                // LOG_DEBUG_FMT("[%s]处理字节: 0x%02X", m_iotSession_UUID.c_str(), static_cast<uint32_t>(byte));  //
                // 打印当前处理的字节（十六进制格式，补零显示两位）
                if (m_frameAssembler->processByte(byte)) {  // 组装完成一帧
                    std::vector<uint8_t> frame = m_frameAssembler->getFrame();
                    std::unique_ptr<FrameBase> parsedFrame;
                    std::string errorMsg;
                    if (FrameParserDispatcher::dispatch(frame, parsedFrame, errorMsg)) {
                        // 处理解析后的帧（交给业务逻辑）
                        handleParsedFrame(parsedFrame);
                    } else {
                        LOG_ERROR_FMT("[%s]帧解析失败:%s", m_iotSession_UUID.c_str(), errorMsg.c_str());
                    }
                    // 关键修改：重置协议相关状态，允许下一帧重新识别协议
                    m_frameAssembler->reset();  // 重置组装器，准备下一帧
                    m_currentProtocol.clear();  // 清除当前协议标识
                    m_frameParser.reset();      // 释放当前解析器
                    m_frameAssembler.reset();   // 释放当前组装器
                    m_recvBuffer.clear();       // 清空接收缓冲区（避免残留数据干扰下一帧识别）
                }
            }
        }
        // 继续读取数据
        startRead();
    } catch (std::exception& e) {
        LOG_ERROR_FMT("[%s]读回调异常:%s", m_iotSession_UUID.c_str(), e.what());
        return;
    }
}

// 逻辑节点构造函数：绑定会话和消息
LogicNode::LogicNode(std::shared_ptr<IotSession> session, std::shared_ptr<MsgNodeRecv> recvnode) : m_logicNode_session(session), m_logicNode_recvnode(recvnode)
{}

std::string IotSession::getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_r(&time, &localTime);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - std::chrono::system_clock::from_time_t(time)).count();
    std::stringstream ss;
    ss << std::put_time(&localTime, "%Y%m%d%H%M%S") << std::setw(3) << std::setfill('0') << ms;
    return ss.str();
}

void IotSession::startRead()
{
    // 示例实现：异步读取数据到缓冲区
    m_iotSession_socket.async_read_some(boost::asio::buffer(m_iotSession_data, MAX_LENGTH),
                                        std::bind(&IotSession::handleRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

// 处理解析后的帧数据
void IotSession::handleParsedFrame(const std::unique_ptr<FrameBase>& frame)
{
    if (frame) {
        LOG_INFO_FMT("[%s]收到解析后的帧，协议类型:%s，消息ID:%d", getUuid().c_str(), frame->protocol.c_str(), frame->msgId);
        if (frame->protocol == "SD_RTU") {
            // SD_RTU协议的帧
            RTUAssembler rtuAssembler;  // 创建RTUAssembler实例，使用其assembleResponse生成响应帧
            std::vector<uint8_t> responseFrame = rtuAssembler.assembleResponse(*frame);
            if (!responseFrame.empty()) {  // 仅当响应帧有效时发送
                try {
                    std::string responseStr(reinterpret_cast<const char*>(responseFrame.data()), responseFrame.size());
                    std::string responseHex = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(responseFrame.data()), responseFrame.size());
                    LOG_INFO_FMT("[%s]反馈帧内容(hex): %s", getUuid().c_str(), responseHex.c_str());
                    uint32_t feedbackMsgId = 0;  // 根据消息ID确定反馈消息类型
                    if (frame->msgId == MSG_SD_RTU_REGISTER) {
                        feedbackMsgId = MSG_SD_RTU_REGISTER_FEEDBACK;
                    } else if (frame->msgId == MSG_SD_RTU_REPORT_DATA) {
                        feedbackMsgId = MSG_SD_RTU_REPORT_DATA_FEEDBACK;
                    }
                    if (feedbackMsgId != 0) {
                        send(responseStr, feedbackMsgId);
                        LOG_INFO_FMT("[%s]反馈帧已发送，长度:%zu字节", getUuid().c_str(), responseFrame.size());
                    } else {
                        LOG_WARNING_FMT("[%s]未匹配的SD_RTU消息ID:%d，无法发送反馈", getUuid().c_str(), frame->msgId);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR_FMT("[%s]SD_RTU反馈帧处理失败:%s", getUuid().c_str(), e.what());
                }
            } else {
                LOG_WARNING_FMT("[%s]RTUAssembler未生成有效响应帧，消息ID:%d", getUuid().c_str(), frame->msgId);
            }
        } else if (frame->protocol == "HY_LORA") {
            // HY_LORA协议的帧
            LoRaAssembler loraAssembler;  // 创建RTUAssembler实例，使用其assembleResponse生成响应帧
            std::vector<uint8_t> responseFrame = loraAssembler.assembleResponse(*frame);
            if (!responseFrame.empty()) {  // 仅当响应帧有效时发送
                try {
                    std::string responseStr(reinterpret_cast<const char*>(responseFrame.data()), responseFrame.size());
                    std::string responseHex = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(responseFrame.data()), responseFrame.size());
                    LOG_INFO_FMT("[%s]反馈帧内容(hex): %s", getUuid().c_str(), responseHex.c_str());
                    uint32_t feedbackMsgId = 0;  // 根据消息ID确定反馈消息类型
                    if (frame->msgId == MSG_HY_LORA_REGISTER) {
                        feedbackMsgId = MSG_HY_LORA_REGISTER_FEEDBACK;
                    } else if (frame->msgId == MSG_HY_LORA_REPORT_DATA) {
                        feedbackMsgId = MSG_HY_LORA_REPORT_DATA_FEEDBACK;
                    }
                    if (feedbackMsgId != 0) {
                        send(responseStr, feedbackMsgId);
                        LOG_INFO_FMT("[%s]反馈帧已发送，长度:%zu字节", getUuid().c_str(), responseFrame.size());
                    } else {
                        LOG_WARNING_FMT("[%s]未匹配的HY_LORA消息ID:%d，无法发送反馈", getUuid().c_str(), frame->msgId);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR_FMT("[%s]HY_LORA反馈帧处理失败:%s", getUuid().c_str(), e.what());
                }
            } else {
                LOG_WARNING_FMT("[%s]LoRaAssembler未生成有效响应帧，消息ID:%d", getUuid().c_str(), frame->msgId);
            }
        } else if (frame->protocol == "SD_YC") {
            YCAssembler assembler;
            std::vector<uint8_t> responseFrame = assembler.assembleResponse(*frame);
            if (!responseFrame.empty()) {  // 仅当响应帧有效时发送
                try {
                    std::string responseStr(reinterpret_cast<const char*>(responseFrame.data()), responseFrame.size());
                    std::string responseHex = utils::UtilsHex::bytesToHexString(reinterpret_cast<const char*>(responseFrame.data()), responseFrame.size());
                    LOG_INFO_FMT("[%s]反馈帧内容(hex): %s", getUuid().c_str(), responseHex.c_str());
                    uint32_t feedbackMsgId = 0;  // 根据消息ID确定反馈消息类型
                    if (frame->msgId == MSG_SD_YC_SEND_DATA_01 || frame->msgId == MSG_SD_YC_SEND_DATA_02 || frame->msgId == MSG_SD_YC_SEND_DATA_03 ||
                        frame->msgId == MSG_SD_YC_SEND_DATA_04 || frame->msgId == MSG_SD_YC_SEND_DATA_05 || frame->msgId == MSG_SD_YC_SEND_DATA_06 ||
                        frame->msgId == MSG_SD_YC_SEND_DATA_07 || frame->msgId == MSG_SD_YC_SEND_DATA_08 || frame->msgId == MSG_SD_YC_SEND_DATA_21 ||
                        frame->msgId == MSG_SD_YC_SEND_DATA_24 || frame->msgId == MSG_SD_YC_SEND_DATA_25 || frame->msgId == MSG_SD_YC_SEND_DATA_26 ||
                        frame->msgId == MSG_SD_YC_SEND_DATA_28) {
                        feedbackMsgId = MSG_SD_YC_CONFIRM;
                    } else {
                        feedbackMsgId = MSG_SD_YC_DENY;
                    }
                    if (feedbackMsgId != 0) {
                        send(responseStr, feedbackMsgId);
                        LOG_INFO_FMT("[%s]反馈帧已发送，长度:%zu字节", getUuid().c_str(), responseFrame.size());
                    } else {
                        LOG_WARNING_FMT("[%s]未匹配的HY_LORA消息ID:%d，无法发送反馈", getUuid().c_str(), frame->msgId);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR_FMT("[%s]SD_YC反馈帧构建失败:%s", getUuid().c_str(), e.what());
                }
            } else {
                LOG_WARNING_FMT("[%s]YCAssembler未生成有效响应帧，消息ID:%d", getUuid().c_str(), frame->msgId);
            }
        } else {  // 可扩展其他协议的帧处理
            LOG_INFO_FMT("[%s]暂不处理协议:%s的反馈", getUuid().c_str(), frame->protocol.c_str());
        }

        // 调用业务逻辑系统进一步处理（如果需要）
        LogicSystem::getInstance().handleFrame(shared_from_this(), const_cast<std::unique_ptr<FrameBase>&>(frame));

    } else {
        LOG_WARNING_FMT("[%s]解析后的帧为空", getUuid().c_str());
    }
}