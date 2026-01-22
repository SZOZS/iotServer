#pragma once
#ifndef CSERVER_H
#define CSERVER_H

#include <boost/asio.hpp>
#include <memory.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../configs/PortProtocolConfig.h"

#include "IotSession.h"
#include "WebSession.h"

class CServer
{
public:
    // CServer(boost::asio::io_context& io_context_param);
    explicit CServer(boost::asio::io_context& io_context_param);
    ~CServer();

    // 禁用拷贝/移动
    CServer(const CServer&) = delete;
    CServer& operator=(const CServer&) = delete;
    CServer(CServer&&) = delete;
    CServer& operator=(CServer&&) = delete;

    void startAccept();
    void clearIotSession(std::string);
    void clearWebSession(std::string);

private:
    void startAcceptForAcceptor(size_t acceptor_idx);                                                                     // 启动指定acceptor的监听
    void handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> session, const boost::system::error_code& ec);  // 处理CoAP连接回调
    void handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> session, const boost::system::error_code& ec);  // 处理WebSocket连接回调

    // 通用日志打印模板函数
    template <typename SessionType>
    void logConnectSuccess(size_t acceptor_idx, std::shared_ptr<SessionType> session, const std::string& protocolType);

    // 成员变量
    boost::asio::io_context& io_context;                     // IO上下文（核心）
    std::vector<short> m_ports;                              // 监听端口列表
    std::vector<boost::asio::ip::tcp::acceptor> _acceptors;  // Acceptor容器列表：每个端口对应一个acceptor（与m_ports一一对应）

    // 协议映射
    std::unordered_map<short, std::string> port_proto_map_;                                                            // 二级映射：端口→大类
    std::unordered_map<short, std::unordered_map<std::string, std::vector<std::string>>> port_category_protocal_map_;  // 三级映射：端口→大类→子协议

    // Session管理
    std::mutex _mutex;
    std::map<std::string, std::shared_ptr<IotSession>> _iot_sessions;  // IoT会话容器
    std::map<std::string, std::shared_ptr<WebSession>> _web_sessions;  // WebSocket会话容器
};

#endif  // CSERVER_H