#pragma once
#ifndef CSERVER_H
#define CSERVER_H

#include <boost/asio.hpp>
#include <map>
#include <memory.h>
#include <mutex>
#include <vector>

class IotSession;
class WebSession;

class CServer
{
public:
    CServer(boost::asio::io_context& io_context, const std::vector<short>& ports);
    ~CServer();
    void clearIotSession(std::string);
    void clearWebSession(std::string);

private:
    void handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> new_session, const boost::system::error_code& error);
    void handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> new_session, const boost::system::error_code& error);
    void startAccept();
    void startAcceptForAcceptor(size_t acceptor_idx);
    std::vector<short> m_ports;
    std::vector<boost::asio::ip::tcp::acceptor> _acceptors;
    std::map<std::string, std::shared_ptr<IotSession>> _iot_sessions;
    std::map<std::string, std::shared_ptr<WebSession>> _web_sessions;
    std::mutex _mutex;
};

#endif  // CSERVER_H