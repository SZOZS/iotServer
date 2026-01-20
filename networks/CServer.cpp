#include <iostream>

#include "../logs/Logger.h"

#include "AsioIOServicePool.h"
#include "CServer.h"
#include "WebSession.h"  // 引入WebSession

CServer::CServer(boost::asio::io_context& io_context) : io_context_(io_context)
{
    _acceptors.reserve(ports.size());
    for (short port : ports) {
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

CServer::~CServer()
{
    for (short port : m_ports) {
        LOG_WARNING_FMT("端口[%hd]已销毁", port);
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
    short port = m_ports[acceptor_idx];
    auto& io_context = AsioIOServicePool::getInstance().getIOService();

    if (port == 8234) {
        std::shared_ptr<IotSession> new_iot_session = std::make_shared<IotSession>(io_context, this);
        _acceptors[acceptor_idx].async_accept(new_iot_session->getSocket(), std::bind(&CServer::handleIotAccept, this, acceptor_idx, new_iot_session, std::placeholders::_1));
    } else {
        std::shared_ptr<WebSession> new_web_session = std::make_shared<WebSession>(io_context, this);
        _acceptors[acceptor_idx].async_accept(new_web_session->getSocket(), std::bind(&CServer::handleWebAccept, this, acceptor_idx, new_web_session, std::placeholders::_1));
    }
}

void CServer::handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> new_session, const boost::system::error_code& error)
{
    short port = m_ports[acceptor_idx];
    std::string uuidStr = new_session->getUuid();
    if (!error) {
        try {
            auto remote_eq = new_session->getSocket().remote_endpoint();
            LOG_INFO_FMT("[%s]从IoT端口[%d]连接成功: %s:%hu", uuidStr.c_str(), port, remote_eq.address().to_string().c_str(), remote_eq.port());
        } catch (const boost::system::system_error& e) {
            LOG_ERROR_FMT("[%s]连接信息获取失败: %s", uuidStr.c_str(), e.what());
        }
        new_session->start();
        std::lock_guard<std::mutex> lock(_mutex);
        _iot_sessions.insert({uuidStr, new_session});
    } else {
        LOG_ERROR_FMT("IoT端口[%d]接受连接失败: %s", port, error.message().c_str());
    }
    startAcceptForAcceptor(acceptor_idx);
}

void CServer::handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> new_session, const boost::system::error_code& error)
{
    short port = m_ports[acceptor_idx];
    std::string uuidStr = new_session->getUuid();
    if (!error) {
        try {
            auto remote_eq = new_session->getSocket().remote_endpoint();
            LOG_INFO_FMT("[%s]从Web端口[%d]连接成功: %s:%hu", uuidStr.c_str(), port, remote_eq.address().to_string().c_str(), remote_eq.port());
        } catch (const boost::system::system_error& e) {
            LOG_ERROR_FMT("[%s]连接信息获取失败: %s", uuidStr.c_str(), e.what());
        }
        new_session->start();
        std::lock_guard<std::mutex> lock(_mutex);
        _web_sessions.insert({uuidStr, new_session});
    } else {
        LOG_ERROR_FMT("Web端口[%d]接受连接失败: %s", port, error.message().c_str());
    }
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