#include <iostream>

#include "../configs/ConfigGlobal.h"
#include "../configs/PortProtocolConfig.h"
#include "../logs/Logger.h"

#include "AsioIOServicePool.h"
#include "CServer.h"

CServer::CServer(boost::asio::io_context& io_context) : io_context_(io_context)
{
    // 1. 从配置获取端口-协议映射
    port_proto_map_ = PortProtocolConfig::getInstance().getPortToCategoryMap();

    // 2. 提取所有端口，初始化m_ports
    for (const auto& pair : port_proto_map_) {
        m_ports.push_back(pair.first);
    }

    // 3. 初始化Acceptor：为每个端口创建一个acceptor
    _acceptors.reserve(m_ports.size());
    for (short port : m_ports) {
        try {
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
            _acceptors.emplace_back(io_context, endpoint.protocol());
            auto& acceptor = _acceptors.back();
            acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor.bind(endpoint);
            acceptor.listen();
            LOG_INFO_FMT("端口[%d]初始化成功", port);
        } catch (const boost::system::system_error& e) {
            LOG_ERROR_FMT("端口[%d]初始化失败: %s", port, e.what());
            throw;
        }
    }
    startAccept();
}

// CServer::~CServer()
// {
//     for (short port : m_ports) {
//         LOG_WARNING_FMT("端口[%hd]已销毁", port);
//     }
// }
CServer::~CServer()
{
    // 自定义析构逻辑，比如关闭所有acceptor
    for (auto& acceptor : _acceptors) {
        boost::system::error_code ec;
        acceptor.close(ec);
        if (ec) {
            LOG_ERROR_FMT("关闭Acceptor失败：%s", ec.message().c_str());
        }
    }
}

void CServer::startAccept()
{
    for (size_t i = 0; i < _acceptors.size(); ++i) {
        startAcceptForAcceptor(i);
    }
}

void CServer::startAcceptForAcceptor(size_t acceptor_idx)
{
    // 边界校验：acceptor索引超出范围
    if (acceptor_idx >= m_ports.size() || acceptor_idx >= _acceptors.size()) {
        LOG_ERROR_FMT("acceptor_idx[%zu]超出范围（端口数：%zu，Acceptor数：%zu）", acceptor_idx, m_ports.size(), _acceptors.size());
        return;
    }

    // 1. 获取当前acceptor对应的端口
    short port = m_ports[acceptor_idx];

    // 2. 从PortProtocolConfig获取端口→协议大类映射（核心修改）
    // std::unordered_map<short, std::string> portProtoMap = PortProtocolConfig::getInstance().getPortToCategoryMap();
    // std::string protocolType = "";
    // auto it = portProtoMap.find(port);
    // if (it != portProtoMap.end()) {
    //     protocolType = it->second;
    // }
    // 2. 从映射中获取协议类型
    std::string protocolType = "";
    auto it = port_proto_map_.find(port);
    if (it != port_proto_map_.end()) {
        protocolType = it->second;
    }

    // 3. 获取IO服务（原有逻辑保留）
    // auto& io_context = AsioIOServicePool::getInstance().getIOService();
    auto& io_context = io_context_;  // 直接使用类成员的io_context

    // 4. 根据协议类型创建不同的Session（替换原硬编码端口判断）
    if (protocolType == "CoAP") {
        // CoAP协议：绑定IotSession
        std::shared_ptr<IotSession> new_iot_session = std::make_shared<IotSession>(io_context, this);
        _acceptors[acceptor_idx].async_accept(new_iot_session->getSocket(), std::bind(&CServer::handleIotAccept, this, acceptor_idx, new_iot_session, std::placeholders::_1));
        LOG_DEBUG_FMT("端口[%d]（acceptor_idx[%zu]）绑定CoAP协议，创建IotSession", port, acceptor_idx);
    } else if (protocolType == "WebSocket") {
        // WebSocket协议：绑定WebSession
        std::shared_ptr<WebSession> new_web_session = std::make_shared<WebSession>(io_context, this);
        _acceptors[acceptor_idx].async_accept(new_web_session->getSocket(), std::bind(&CServer::handleWebAccept, this, acceptor_idx, new_web_session, std::placeholders::_1));
        LOG_DEBUG_FMT("端口[%d]（acceptor_idx[%zu]）绑定WebSocket协议，创建WebSession", port, acceptor_idx);
    } else {
        // 容错：未知协议/未配置端口，打印日志并延迟重试
        LOG_ERROR_FMT("端口[%d]（acceptor_idx[%zu]）未配置协议类型（或配置非CoAP/WebSocket），无法创建会话", port, acceptor_idx);
        // 用io_context.post避免递归栈溢出，优雅重试
        io_context.post(std::bind(&CServer::startAcceptForAcceptor, this, acceptor_idx));
        return;
    }
}

// CoAP连接回调实现
// void CServer::handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> new_session, const boost::system::error_code& error)
// {
//     short port = m_ports[acceptor_idx];
//     std::string uuidStr = new_session->getUuid();
//     if (!error) {
//         try {
//             auto remote_eq = new_session->getSocket().remote_endpoint();
//             LOG_INFO_FMT("[%s]从IoT端口[%d]连接成功: %s:%hu", uuidStr.c_str(), port, remote_eq.address().to_string().c_str(), remote_eq.port());
//         } catch (const boost::system::system_error& e) {
//             LOG_ERROR_FMT("[%s]连接信息获取失败: %s", uuidStr.c_str(), e.what());
//         }
//         new_session->start();
//         std::lock_guard<std::mutex> lock(_mutex);
//         _iot_sessions.insert({uuidStr, new_session});
//     } else {
//         LOG_ERROR_FMT("IoT端口[%d]接受连接失败: %s", port, error.message().c_str());
//     }
//     startAcceptForAcceptor(acceptor_idx);
// }
void CServer::handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> session, const boost::system::error_code& ec)
{
    if (!ec) {
        LOG_DEBUG_FMT("CoAP连接成功，acceptor_idx[%zu]", acceptor_idx);
        session->start();  // 启动Session处理逻辑
    } else {
        LOG_ERROR_FMT("CoAP连接失败，acceptor_idx[%zu]：%s", acceptor_idx, ec.message().c_str());
    }
    // 重新启动监听，等待下一个连接
    startAcceptForAcceptor(acceptor_idx);
}

// WebSocket连接回调实现
// void CServer::handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> new_session, const boost::system::error_code& error)
// {
//     short port = m_ports[acceptor_idx];
//     std::string uuidStr = new_session->getUuid();
//     if (!error) {
//         try {
//             auto remote_eq = new_session->getSocket().remote_endpoint();
//             LOG_INFO_FMT("[%s]从Web端口[%d]连接成功: %s:%hu", uuidStr.c_str(), port, remote_eq.address().to_string().c_str(), remote_eq.port());
//         } catch (const boost::system::system_error& e) {
//             LOG_ERROR_FMT("[%s]连接信息获取失败: %s", uuidStr.c_str(), e.what());
//         }
//         new_session->start();
//         std::lock_guard<std::mutex> lock(_mutex);
//         _web_sessions.insert({uuidStr, new_session});
//     } else {
//         LOG_ERROR_FMT("Web端口[%d]接受连接失败: %s", port, error.message().c_str());
//     }
//     startAcceptForAcceptor(acceptor_idx);
// }
void CServer::handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> session, const boost::system::error_code& ec)
{
    if (!ec) {
        LOG_DEBUG_FMT("WebSocket连接成功，acceptor_idx[%zu]", acceptor_idx);
        session->start();  // 启动Session处理逻辑
    } else {
        LOG_ERROR_FMT("WebSocket连接失败，acceptor_idx[%zu]：%s", acceptor_idx, ec.message().c_str());
    }
    // 重新启动监听，等待下一个连接
    startAcceptForAcceptor(acceptor_idx);
}

void CServer::clearIotSession(std::string uuid)
{
    LOG_WARNING_FMT("[%s]已清除", uuid.c_str());
    std::lock_guard<std::mutex> lock(_mutex);
    _iot_sessions.erase(uuid);
}

void CServer::clearWebSession(std::string uuid)
{
    LOG_WARNING_FMT("[Web会话%s]已清除", uuid.c_str());
    std::lock_guard<std::mutex> lock(_mutex);
    _web_sessions.erase(uuid);
}