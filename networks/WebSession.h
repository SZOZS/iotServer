#pragma once
#ifndef WEB_SESSION_H
#define WEB_SESSION_H

#include <boost/asio.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <memory>
#include <mutex>
#include <queue>

#include "../nodes/MsgNodeSend.h"
#include "../thrid_partys/json.hpp"

using json = nlohmann::json;

class CServer;
class WebSessionManager;

class WebSession : public std::enable_shared_from_this<WebSession>
{
public:
    WebSession(boost::asio::io_context& io_context, CServer* server);
    ~WebSession();

    boost::asio::ip::tcp::socket& getSocket();
    std::string& getUuid();
    int getOwnerId() const { return _owner_id; }
    bool isVerified() const { return _is_verified; }

    void start();
    void send(const json& data);
    void close();

private:
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error);
    void processMsg(const std::string& msg);
    void handleVueLogin(const json& data);
    void handlePhpPush(const json& data);
    bool verifyOwnerAccount(int owner_id, const std::string& account_id);
    bool handleWebSocketHandshake(const std::string& request);
    bool isPossibleJson(const std::string& msg);
    std::string decodeWebSocketFrame(const std::string& frame_data);
    std::string encodeWebSocketFrame(const std::string& payload);

    boost::asio::ip::tcp::socket _socket;
    CServer* _server;
    std::string _uuid;
    char _recv_buf[1024];
    std::string _recv_data;
    std::queue<std::shared_ptr<MsgNodeSend>> _send_que;
    std::mutex _send_mutex;
    int _owner_id = -1;
    int _account_id = -1;
    int _failure_time = -1;
    bool _is_handshaked = false;
    bool _is_verified = false;
};

#endif  // WEB_SESSION_H