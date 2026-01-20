#pragma once
#ifndef CSERVER_H
#define CSERVER_H

#include <boost/asio.hpp>
#include <map>
#include <memory.h>
#include <mutex>
#include <vector>

#include "IotSession.h"
#include "WebSession.h"

// 前向声明配置类
class PortProtocolConfig;

class CServer
{
public:
    CServer(boost::asio::io_context& io_context);
    ~CServer();
    void clearIotSession(std::string);
    void clearWebSession(std::string);

    // 核心方法：启动指定acceptor的监听
    void startAcceptForAcceptor(size_t acceptor_idx);
    // 处理CoAP连接回调
    void handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> session, const boost::system::error_code& ec);
    // 处理WebSocket连接回调
    void handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> session, const boost::system::error_code& ec);

private:
    // 成员变量（匹配构造函数初始化）
    boost::asio::io_context& io_context_;
    // Acceptor容器：每个端口对应一个acceptor
    std::vector<boost::asio::ip::tcp::acceptor> _acceptors;
    // 端口列表：从配置中加载
    std::vector<short> m_ports;
    // 端口→协议类型映射
    std::unordered_map<short, std::string> port_proto_map_;

    void startAccept();
    std::map<std::string, std::shared_ptr<IotSession>> _iot_sessions;
    std::map<std::string, std::shared_ptr<WebSession>> _web_sessions;
    std::mutex _mutex;
};

#endif  // CSERVER_H