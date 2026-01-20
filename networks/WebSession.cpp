#include "WebSession.h"

#include <openssl/sha.h>

#include <cstdint>

#include "../handlers/mysql/MySQLConnectionPool.h"
#include "../logics/web_socket/WebSessionManager.h"
#include "../logs/Logger.h"
#include "../utils/UtilsHex.h"
#include "CServer.h"

WebSession::WebSession(boost::asio::io_context& io_context, CServer* server) : _socket(io_context), _server(server) {
  boost::uuids::uuid uuid = boost::uuids::random_generator()();
  _uuid = boost::uuids::to_string(uuid);
}

WebSession::~WebSession() { LOG_INFO_FMT("[%s]会话销毁", _uuid.c_str()); }

boost::asio::ip::tcp::socket& WebSession::getSocket() { return _socket; }

std::string& WebSession::getUuid() { return _uuid; }

void WebSession::start() {
  _socket.async_read_some(boost::asio::buffer(_recv_buf, 1024), std::bind(&WebSession::handleRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void WebSession::send(const json& data) {
  std::lock_guard<std::mutex> lock(_send_mutex);
  // 1. 生成JSON字符串(无需添加换行符)
  std::string payload = data.dump();
  // 2. 封装为WebSocket帧
  std::string frame = encodeWebSocketFrame(payload);
  // 3. 发送帧数据
  _send_que.push(std::make_shared<MsgNodeSend>(frame.data(), frame.size(), 0, _uuid));

  if (_send_que.size() == 1) {
    auto& node = _send_que.front();
    boost::asio::async_write(_socket, boost::asio::buffer(node->m_msgNodeSend_data, node->m_msgNodeSend_totalLen),
                             std::bind(&WebSession::handleWrite, shared_from_this(), std::placeholders::_1));
  }
}

void WebSession::close() {
  if (_owner_id != -1) WebSessionManager::getInstance().removeOwnerSession(_owner_id, shared_from_this());
  _socket.close();
  _server->clearWebSession(_uuid);
}

void WebSession::handleRead(const boost::system::error_code& error, size_t bytes_transferred) {
  if (error) {
    LOG_ERROR_FMT("[%s]读取失败:%s", _uuid.c_str(), error.message().c_str());
    close();
    return;
  }

  _recv_data.append(_recv_buf, bytes_transferred);

  // 第一步：处理WebSocket握手(未握手时)
  if (!_is_handshaked) {
    // 查找HTTP请求结束标记(\r\n\r\n)
    size_t handshake_end = _recv_data.find("\r\n\r\n");
    if (handshake_end != std::string::npos) {
      // 提取完整的HTTP握手请求
      std::string handshake_req = _recv_data.substr(0, handshake_end + 4);
      _recv_data.erase(0, handshake_end + 4);

      // 解析并响应握手请求
      if (handleWebSocketHandshake(handshake_req)) {
        _is_handshaked = true;
        LOG_INFO_FMT("[%s]WebSocket握手成功", _uuid.c_str());
      } else {
        LOG_ERROR_FMT("[%s]WebSocket握手失败", _uuid.c_str());
        close();
        return;
      }
    } else {
      // 未接收到完整的握手请求，继续等待
      _socket.async_read_some(boost::asio::buffer(_recv_buf, 1024), std::bind(&WebSession::handleRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
      return;
    }
  }

  // 第二步：已握手，处理JSON消息(原逻辑)
  LOG_INFO("等待接收数据...");
  // 对接收的原始数据进行WebSocket帧解码
  std::string decoded_data = decodeWebSocketFrame(_recv_data);
  if (!decoded_data.empty()) {
    // 将解码后的有效数据追加到缓冲区(可能存在多帧合并)
    _recv_data = decoded_data;

    // 按换行符分割消息
    size_t pos;
    while ((pos = _recv_data.find('\n')) != std::string::npos) {
      LOG_INFO("数据接收中...");
      std::string msg = _recv_data.substr(0, pos);
      _recv_data.erase(0, pos + 1);
      processMsg(msg);
    }
  } else {
    // 解码失败或无有效数据，清空缓冲区避免残留
    _recv_data.clear();
  }

  _socket.async_read_some(boost::asio::buffer(_recv_buf, 10240), std::bind(&WebSession::handleRead, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void WebSession::handleWrite(const boost::system::error_code& error) {
  if (error) {
    LOG_ERROR_FMT("[%s]写入失败:%s", _uuid.c_str(), error.message().c_str());
    close();
    return;
  }

  std::lock_guard<std::mutex> lock(_send_mutex);
  _send_que.pop();
  if (!_send_que.empty()) {
    auto& node = _send_que.front();
    boost::asio::async_write(_socket, boost::asio::buffer(node->m_msgNodeSend_data, node->m_msgNodeSend_totalLen),
                             std::bind(&WebSession::handleWrite, shared_from_this(), std::placeholders::_1));
  }
}

void WebSession::processMsg(const std::string& msg) {
  if (!isPossibleJson(msg)) {
    LOG_ERROR_FMT("[%s]消息不是JSON格式: %s", _uuid.c_str(), msg.c_str());
    return;
  }
  // 打印原始消息(可见字符)
  LOG_DEBUG_FMT("[%s]待解析原始消息: %s", _uuid.c_str(), msg.c_str());
  // // 打印十六进制(方便查看不可见字符或乱码)
  // std::string msgHex = utils::UtilsHex::bytesToHexString(msg.c_str(), msg.size());
  // LOG_DEBUG_FMT("[%s]待解析消息十六进制: %s", _uuid.c_str(), msgHex.c_str());

  try {
    json data = json::parse(msg);
    // LOG_DEBUG_FMT("[%s]解析后的JSON数据: %s", _uuid.c_str(), data.dump(2).c_str());

    std::string type = data["type"];
    if (type == "vue_login") {
      handleVueLogin(data);
    } else if (type == "php_push") {
      // std::string client_ip = _socket.remote_endpoint().address().to_string();
      // const auto& whitelist = ConfigGlobal::getInstance().getConfigPhpPush().ip_whitelist;

      // if (whitelist.find(client_ip) == whitelist.end()) {
      //   LOG_WARNING_FMT("[%s]IP %s 不在白名单，拒绝处理php_push消息", _uuid.c_str(), client_ip.c_str());
      //   send(json{{"type", "php_push"}, {"code", 403}, {"message", "IP未授权"}, {"data", json({})}});
      //   close();  // 可选：直接断开未授权连接
      //   return;
      // }
      handlePhpPush(data);
    } else {
      LOG_WARNING_FMT("[%s]未知消息类型:%s", _uuid.c_str(), type.c_str());
    }
  } catch (const std::exception& e) {
    LOG_ERROR_FMT("[%s]消息解析失败:%s,原始数据:%s", _uuid.c_str(), e.what(), msg.c_str());
  }
}

void WebSession::handleVueLogin(const json& data) {
  if (_is_verified) {
    send(json{{"type", "login_resp"}, {"code", 400}, {"message", "已登录"}, {"data", json({})}});
    return;
  }
  // 提取并校验 token 格式(去掉 Bearer 前缀)
  std::string tokenStr;
  try {
    tokenStr = data["token"].get<std::string>(); // 获取原始 token 字符串
  } catch (const std::exception& e) {
    // 若 data["token"] 不存在或不是字符串，直接返回校验失败
    send(json{{"type", "login_resp"}, {"code", 401}, {"message", "token格式错误(非字符串)"}, {"data", json({})}});
    return;
  }
  // 校验是否以 Bearer 开头，且长度足够
  const std::string bearerPrefix = "Bearer "; // 注意末尾有空格
  std::string actualToken;
  if (tokenStr.size() < bearerPrefix.size() || tokenStr.substr(0, bearerPrefix.size()) != bearerPrefix) {
    send(json{{"type", "login_resp"}, {"code", 401}, {"message", "token格式错误(需以 'Bearer ' 开头)"}, {"data", json({})}});
    return;
  }
  // 提取 Bearer 前缀后的实际 token
  actualToken = tokenStr.substr(bearerPrefix.size());

  const std::vector<std::string> fields = {"people_id as account_id", "relate_id as owner_id", "failure_time"};
  time_t currentTime = time(nullptr);
  const std::vector<std::vector<std::string>> conditions = {
      {"token", actualToken}, {"type", "application"}, {"relate_type", "owner"}, {"failure_time", ">=", std::to_string(currentTime)}};
  std::map<std::string, std::string> order_by = {{"id", "DESC"}};

  auto mysqlPool = MySQLConnectionPool::ConnectionGuard(MySQLConnectionPool::getInstance().getConnection());
  if (!mysqlPool.get()) {
    LOG_ERROR("获取有效MySQL连接失败");
  } else {
    const std::map<std::string, std::string> result = mysqlPool->queryOneMax("token_jwts", fields, conditions, order_by);
    if (result.empty()) {
      LOG_DEBUG("获取数据 失败");
      send(json{{"type", "login_resp"}, {"code", 401}, {"message", "校验失败(token无效或已过期)"}, {"data", json({})}});
      return;
    } else {
      int owner_id = std::atoi(result.at("owner_id").c_str());
      int account_id = std::atoi(result.at("account_id").c_str());
      int failure_time = std::atoi(result.at("failure_time").c_str());
      auto& manager = WebSessionManager::getInstance();
      auto current_session = shared_from_this();
      manager.bindOwnerSession(owner_id, current_session);  
      current_session->_owner_id = owner_id;
      current_session->_account_id = account_id;
      current_session->_failure_time = failure_time;
      current_session->_is_verified = true;
      current_session->send(json{{"type", "login_resp"},
                                 {"code", 200},
                                 {"message", "登录成功"},
                                 {"data", json({{"session_id", current_session->_uuid}, {"owner_id", owner_id}, {"account_id", account_id}, {"failure_time", failure_time}})}});
    }
  }
}

void WebSession::handlePhpPush(const json& data) {
  int owner_id = data["data"]["owner_id"];
  std::vector<std::string> interfaces = data["data"]["interface_list"];

  auto sessions = WebSessionManager::getInstance().getOwnerSessions(owner_id);
  if (sessions.empty()) {
    LOG_WARNING_FMT("[业主%d]无任何会话，忽略推送", owner_id);
    return;
  }
  // 遍历所有会话，发送数据(仅给已验证的会话)
  int sent_count = 0;
  for (const auto& session : sessions) {
    if (session && session->isVerified()) {
      session->send(json{{"type", "php_push"}, {"code", 200}, {"message", "success"}, {"data", json{{"action_type", "refresh_interface"}, {"interfaces", interfaces}}}});
      sent_count++;
      LOG_INFO_FMT("[业主%d]向会话%s推送接口: %s", owner_id, session->getUuid().c_str(), json(interfaces).dump().c_str());
    }
  }
  LOG_INFO_FMT("[业主%d]推送完成，共向%d个有效会话发送数据(总会话数: %zu)", owner_id, sent_count, sessions.size());
}

bool WebSession::handleWebSocketHandshake(const std::string& request) {
  // 1. 从请求中提取Sec-WebSocket-Key
  std::string key;
  size_t key_pos = request.find("Sec-WebSocket-Key: ");
  if (key_pos == std::string::npos) {
    return false;
  }
  key_pos += 19;  // "Sec-WebSocket-Key: "的长度
  size_t key_end = request.find("\r\n", key_pos);
  if (key_end == std::string::npos) {
    return false;
  }
  std::string sec_key = request.substr(key_pos, key_end - key_pos);

  // 2. 生成Sec-WebSocket-Accept(WebSocket协议规定的算法)
  const std::string magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string accept_str = sec_key + magic_string;

  // 计算SHA-1哈希
  unsigned char sha1_hash[20];
  SHA1((const unsigned char*)accept_str.c_str(), accept_str.size(), sha1_hash);

  // 哈希结果进行Base64编码
  std::string sec_accept = utils::UtilsHex::base64_encode(sha1_hash, 20);

  // 3. 构造握手响应
  std::string response =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
      sec_accept + "\r\n\r\n";

  // 4. 发送响应
  boost::system::error_code ec;
  boost::asio::write(_socket, boost::asio::buffer(response), ec);
  return !ec;
}

bool WebSession::isPossibleJson(const std::string& msg) {
  if (msg.empty()) return false;
  // 简单校验：必须以{开头、}结尾(忽略前后空白)
  size_t start = msg.find_first_not_of(" \t");
  size_t end = msg.find_last_not_of(" \t");
  if (start == std::string::npos || end == std::string::npos) return false;
  return msg[start] == '{' && msg[end] == '}';
}

std::string WebSession::decodeWebSocketFrame(const std::string& frame_data) {
  if (frame_data.empty()) {
    return "";
  }

  size_t pos = 0;
  // 1. 解析第一个字节(FIN + Opcode)
  uint8_t first_byte = static_cast<uint8_t>(frame_data[pos++]);
  // bool fin = (first_byte & 0x80) != 0;  // 是否为最后一帧
  uint8_t opcode = first_byte & 0x0F;   // 帧类型(文本帧为0x1)

  // 处理关闭帧(Opcode=8)
  if (opcode == 0x8) {
    LOG_INFO_FMT("[%s]收到客户端关闭帧，准备断开连接", _uuid.c_str());
    close();  // 主动关闭会话
    return "";
  }

  // 只处理文本帧(Opcode=0x1)，忽略其他类型(如二进制帧、关闭帧等)
  if (opcode != 0x1) {
    LOG_WARNING_FMT("[%s]不支持的WebSocket帧类型(Opcode=%d)", _uuid.c_str(), opcode);
    return "";
  }

  // 2. 解析第二个字节(掩码标志 + payload长度)
  uint8_t second_byte = static_cast<uint8_t>(frame_data[pos++]);
  bool masked = (second_byte & 0x80) != 0;  // 客户端发送的数据必须掩码
  uint64_t payload_len = second_byte & 0x7F;

  // 3. 解析payload长度(根据长度值处理不同字节数)
  if (payload_len == 126) {
    // 长度为126时，后续2字节表示实际长度(16位无符号整数)
    if (pos + 2 > frame_data.size()) {
      LOG_ERROR_FMT("[%s]WebSocket帧长度字段不完整", _uuid.c_str());
      return "";
    }
    payload_len = (static_cast<uint8_t>(frame_data[pos]) << 8) | static_cast<uint8_t>(frame_data[pos + 1]);
    pos += 2;
  } else if (payload_len == 127) {
    // 长度为127时，后续8字节表示实际长度(64位无符号整数，暂不处理超大长度)
    LOG_WARNING_FMT("[%s]不支持超过65535字节的WebSocket帧", _uuid.c_str());
    return "";
  }

  // 4. 解析掩码密钥(4字节，仅当masked为true时存在)
  uint8_t mask[4] = {0};
  if (masked) {
    if (pos + 4 > frame_data.size()) {
      LOG_ERROR_FMT("[%s]WebSocket帧掩码字段不完整", _uuid.c_str());
      return "";
    }
    for (int i = 0; i < 4; ++i) {
      mask[i] = static_cast<uint8_t>(frame_data[pos++]);
    }
  }

  // 5. 解析payload并应用掩码解码
  if (pos + payload_len > frame_data.size()) {
    LOG_ERROR_FMT("[%s]WebSocket帧payload不完整", _uuid.c_str());
    return "";
  }
  std::string payload;
  payload.reserve(payload_len);
  for (uint64_t i = 0; i < payload_len; ++i) {
    uint8_t byte = static_cast<uint8_t>(frame_data[pos + i]);
    if (masked) {
      byte ^= mask[i % 4];  // 掩码解码(异或操作)
    }
    payload += static_cast<char>(byte);
  }

  return payload;
}

std::string WebSession::encodeWebSocketFrame(const std::string& payload) {
  std::string frame;
  // 第一个字节：FIN=1(帧结束)，Opcode=0x1(文本帧)
  frame.push_back(0x81);

  // 处理 payload 长度(第二个字节及后续)
  size_t payload_len = payload.size();
  if (payload_len <= 125) {
    // 长度 <= 125 直接写入
    frame.push_back(static_cast<char>(payload_len));
  } else if (payload_len <= 65535) {
    // 长度 126~65535：先写126，再写2字节长度(大端)
    frame.push_back(126);
    frame.push_back(static_cast<char>((payload_len >> 8) & 0xFF));
    frame.push_back(static_cast<char>(payload_len & 0xFF));
  } else {
    // 长度 >65535：先写127，再写8字节长度(大端)
    frame.push_back(127);
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<char>((payload_len >> (8 * i)) & 0xFF));
    }
  }

  // 添加 payload 数据
  frame.append(payload);
  return frame;
}