#include <algorithm>
#include <iostream>
#include <mutex>

#include "../configs/ConfigGlobal.h"
#include "../configs/PortProtocolConfig.h"
#include "../logs/Logger.h"

#include "AsioIOServicePool.h"
#include "CServer.h"
#include "IotSession.h"
#include "WebSession.h"

// 构造函数：初始化IO上下文、端口-协议映射、Acceptor
CServer::CServer(boost::asio::io_context& io_context_param) : io_context(io_context_param)
{
    // 确保线程池在创建Session前已初始化，避免空指针/未启动问题
    auto& io_pool = AsioIOServicePool::getInstance();
    if (!io_pool.isRunning()) {  // 假设线程池有isRunning()检查状态，无则可省略
        io_pool.start();         // 启动线程池（如创建工作线程、运行io_context）
    }

    // 1. 从配置获取端口-协议大类-子协议的三级映射
    port_category_protocal_map_ = PortProtocolConfig::getInstance().getPortToCategoryMap();

    // ========== 打印 port_category_protocal_map_ 完整内容 ==========
    LOG_DEBUG_FMT_DELAY("===== 端口-协议大类-子协议映射表（共%zu项）=====", port_category_protocal_map_.size());
    if (port_category_protocal_map_.empty()) {
        LOG_DEBUG_DELAY("port_category_protocal_map_ 为空！配置解析可能失败");
    } else {
        // 1. 端口按升序排序
        std::vector<std::pair<short, std::unordered_map<std::string, std::vector<std::string>>>> sorted_ports;
        for (const auto& pair : port_category_protocal_map_) {
            sorted_ports.emplace_back(pair.first, pair.second);
        }
        std::sort(sorted_ports.begin(), sorted_ports.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        // 2. 遍历排序后的端口
        for (const auto& port_pair : sorted_ports) {
            short port = port_pair.first;
            const auto& category_map = port_pair.second;

            // 大类按名称升序排序
            std::vector<std::pair<std::string, std::vector<std::string>>> sorted_categories;
            for (const auto& category_pair : category_map) {
                sorted_categories.emplace_back(category_pair.first, category_pair.second);
            }
            std::sort(sorted_categories.begin(), sorted_categories.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

            // 3. 遍历大类+多子协议
            for (const auto& category_pair : sorted_categories) {
                std::string category = category_pair.first;
                const auto& sub_protocols = category_pair.second;  // 子协议列表

                // 打印大类标题
                LOG_DEBUG_FMT_DELAY("端口[%d] → 协议大类[%s]（子协议数：%zu）", port, category.c_str(), sub_protocols.size());

                // 遍历打印每个子协议
                for (size_t i = 0; i < sub_protocols.size(); ++i) {
                    LOG_DEBUG_FMT_DELAY("  └─ 子协议[%zu]：%s", i + 1, sub_protocols[i].c_str());
                }

                // ========== 初始化 port_proto_map_（端口→协议大类） ==========
                port_proto_map_[port] = category;
            }
        }
    }
    LOG_DEBUG_DELAY("===========================================");

    // 2. 提取所有端口，初始化m_ports
    for (const auto& pair : port_category_protocal_map_) {
        m_ports.push_back(pair.first);
    }

    // 3. 初始化Acceptor：为每个端口创建一个TCP acceptor并绑定/监听
    _acceptors.reserve(m_ports.size());
    for (short port : m_ports) {
        try {
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
            // 1. 创建Acceptor对象（绑定IO上下文+协议类型）
            // 创建 Acceptor 对象 - 关联 Boost.Asio 的 io_context（IO 事件驱动核心），指定协议类型（如 TCP v4）；
            _acceptors.emplace_back(this->io_context, endpoint.protocol());
            auto& acceptor = _acceptors.back();
            // 2. 设置端口复用（可选但常用）
            // 设置 reuse_address - 允许端口复用（比如服务器重启时，无需等待端口释放就能重新绑定，避免 “端口被占用” 错误）；
            acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            // 3. 绑定到指定端口（把接待员安排到指定地址）
            // bind(endpoint) - 把 Acceptor 绑定到「指定 IP + 端口」（比如 0.0.0.0 : 8234），确定监听的地址；
            acceptor.bind(endpoint);
            // 4. 开始监听（接待员上岗，等待客户）
            // listen() - 让 Acceptor 进入 “监听状态”，开始等待客户端连接（底层调用系统的 listen() 系统调用）；
            acceptor.listen();
            LOG_INFO_FMT_DELAY("端口[%d]初始化成功（协议大类：%s）", port, port_proto_map_[port].c_str());
        } catch (const boost::system::system_error& e) {
            LOG_ERROR_FMT_DELAY("端口[%d]初始化失败: %s", port, e.what());
            throw;
        }
    }
    startAccept();  // 启动异步接受连接
}

// 「端口→协议→Session」的解耦：不同协议对应不同 Session，后续协议扩展只需新增分支；
// 异步模型：Boost.Asio 的async_accept是非阻塞的，不会阻塞主线程。
// 启动异步接受连接（startAccept / startAcceptForAcceptor）
// startAccept()：遍历所有 Acceptor，调用startAcceptForAcceptor为每个端口启动异步接受；
void CServer::startAccept()
{
    for (size_t i = 0; i < _acceptors.size(); ++i) {
        startAcceptForAcceptor(i);  // 为每个Acceptor启动异步接受
    }
}

// startAcceptForAcceptor：
// ① 校验索引合法性；
// ② 根据端口从port_proto_map_获取协议类型；
// ③ 按协议类型创建IotSession（CoAP）或WebSession（WebSocket）；
// ④ 调用async_accept异步等待连接，绑定对应的回调函数（handleIotAccept / handleWebAccept）；
// ⑤ 未知协议时通过io_context.post优雅重试（避免递归栈溢出）。
void CServer::startAcceptForAcceptor(size_t acceptor_idx)
{
    // 1. 边界校验：防止索引越界
    if (acceptor_idx >= m_ports.size() || acceptor_idx >= _acceptors.size()) {
        LOG_ERROR_FMT_DELAY("acceptor_idx[%zu]超出范围（端口数：%zu，Acceptor数：%zu）", acceptor_idx, m_ports.size(), _acceptors.size());
        return;
    }

    // 2. 获取当前端口和对应的协议类型
    short port = m_ports[acceptor_idx];
    std::string protocolType = "";
    auto it = port_proto_map_.find(port);
    if (it != port_proto_map_.end()) {
        protocolType = it->second;
        LOG_DEBUG_FMT_DELAY("端口[%d]（acceptor_idx[%zu]）配置的协议大类：%s", port, acceptor_idx, protocolType.c_str());
    } else {
        LOG_ERROR_FMT_DELAY("端口[%d]（acceptor_idx[%zu]）未在port_proto_map_中找到协议配置", port, acceptor_idx);
    }

    // 3. 根据协议类型创建不同的Session
    auto& io_context = AsioIOServicePool::getInstance().getIOService();  // 高并发场景,通过线程池提升处理能力；
    if (protocolType == "CoAP") {
        // CoAP协议：创建IotSession并启动异步accept
        std::shared_ptr<IotSession> iot_session = std::make_shared<IotSession>(io_context, this, port, protocolType);
        // async_accept（） - 异步等待连接：连接到来时，自动完成 TCP 握手，并调用回调函数（如 handleIotAccept）；
        _acceptors[acceptor_idx].async_accept(iot_session->getSocket(),
                                              std::bind(&CServer::handleIotAccept, this, acceptor_idx, iot_session, std::placeholders::_1));
        LOG_DEBUG_FMT_DELAY("端口[%d]（acceptor_idx[%zu]）绑定CoAP协议，创建IotSession", port, acceptor_idx);
    } else if (protocolType == "WebSocket") {
        // WebSocket协议：创建WebSession并启动异步accept
        std::shared_ptr<WebSession> web_session = std::make_shared<WebSession>(io_context, this);
        // async_accept（） - 异步等待连接：连接到来时，自动完成 TCP 握手，并调用回调函数（如 handleIotAccept）；
        _acceptors[acceptor_idx].async_accept(web_session->getSocket(),
                                              std::bind(&CServer::handleWebAccept, this, acceptor_idx, web_session, std::placeholders::_1));
        LOG_DEBUG_FMT_DELAY("端口[%d]（acceptor_idx[%zu]）绑定WebSocket协议，创建WebSession", port, acceptor_idx);
    } else {
        // 未知协议：打印日志并延迟重试（优化：仅重试3次，避免无限循环）
        static std::unordered_map<size_t, int> retry_count;
        retry_count[acceptor_idx]++;
        if (retry_count[acceptor_idx] <= 3) {
            LOG_ERROR_FMT_DELAY("端口[%d]（acceptor_idx[%zu]）未配置CoAP/WebSocket协议（当前配置：%s），第%d次重试",
                                port,
                                acceptor_idx,
                                protocolType.c_str(),
                                retry_count[acceptor_idx]);
            // 用io_context.post避免递归栈溢出，优雅重试
            io_context.post(std::bind(&CServer::startAcceptForAcceptor, this, acceptor_idx));
        } else {
            LOG_FATAL_FMT_DELAY("端口[%d]（acceptor_idx[%zu]）协议配置错误，重试3次失败，停止监听", port, acceptor_idx);
            retry_count.erase(acceptor_idx);
        }
        return;
    }
}

// 核心逻辑：
// 连接成功（无错误）：调用session->start() 启动 iot_session/web_session 的业务逻辑（如异步读数据、协议解析）；
// 连接失败：打印错误日志；
// 必做操作：调用startAcceptForAcceptor重新启动异步接受 —— 这是Boost.Asio 异步监听的核心，保证 Acceptor 处理完一个连接后，继续监听下一个连接。
// 注释掉的旧版本：旧版本包含了获取客户端 IP/端口、Session UUID 管理等逻辑，新版本简化为核心流程，可根据需求恢复。
// CoAP连接回调实现
void CServer::handleIotAccept(size_t acceptor_idx, std::shared_ptr<IotSession> iot_session, const boost::system::error_code& error)
{
    if (!error) {
        LOG_DEBUG_FMT_DELAY("CoAP连接成功，acceptor_idx[%zu]", acceptor_idx);
        std::string uuidStr = iot_session->getUuid();
        logConnectSuccess(acceptor_idx, iot_session, "IoT");  // 连接信息日志
        iot_session->start();                                 // 启动Session的业务处理逻辑（如读数据、解析协议）
        std::lock_guard<std::mutex> lock(_mutex);
        _iot_sessions.insert({uuidStr, iot_session});
    } else {
        LOG_ERROR_FMT_DELAY("CoAP连接失败，acceptor_idx[%zu]：%s", acceptor_idx, error.message().c_str());
    }
    // 重新启动监听，等待下一个连接（核心：保证Acceptor持续监听）
    startAcceptForAcceptor(acceptor_idx);
}
// WebSocket连接回调实现
void CServer::handleWebAccept(size_t acceptor_idx, std::shared_ptr<WebSession> web_session, const boost::system::error_code& error)
{
    if (!error) {
        LOG_DEBUG_FMT_DELAY("WebSocket连接成功，acceptor_idx[%zu]", acceptor_idx);
        std::string uuidStr = web_session->getUuid();
        logConnectSuccess(acceptor_idx, web_session, "Web");  // 连接信息日志
        web_session->start();                                 // 启动Session处理逻辑
        std::lock_guard<std::mutex> lock(_mutex);
        _web_sessions.insert({uuidStr, web_session});
    } else {
        LOG_ERROR_FMT_DELAY("WebSocket连接失败，acceptor_idx[%zu]：%s", acceptor_idx, error.message().c_str());
    }
    // 重新启动监听，等待下一个连接
    startAcceptForAcceptor(acceptor_idx);
}

// 核心逻辑：
// 根据 Session 的 UUID 从_iot_sessions / _web_sessions（Session 管理容器）中删除对应的 Session；
// std::lock_guard<std::mutex>：保证多线程下 Session容器的线程安全（网络事件是异步的，可能多线程操作容器）。
// 清除IoT会话
void CServer::clearIotSession(std::string uuid)
{
    LOG_WARNING_FMT("[%s]已清除", uuid.c_str());
    std::lock_guard<std::mutex> lock(_mutex);
    _iot_sessions.erase(uuid);
}
// 清除WebSocket会话
void CServer::clearWebSession(std::string uuid)
{
    LOG_WARNING_FMT("[Web会话%s]已清除", uuid.c_str());
    std::lock_guard<std::mutex> lock(_mutex);
    _web_sessions.erase(uuid);
}

// 通用连接成功日志打印函数（模板）
template <typename SessionType>
void CServer::logConnectSuccess(size_t acceptor_idx, std::shared_ptr<SessionType> session, const std::string& protocolType)
{
    short port = m_ports[acceptor_idx];
    std::string uuidStr = session->getUuid();

    std::string clientIp = "unknown";
    uint16_t clientPort = 0;
    try {
        auto remote_eq = session->getSocket().remote_endpoint();
        clientIp = remote_eq.address().to_string();
        clientPort = remote_eq.port();
    } catch (const boost::system::system_error& e) {
        LOG_ERROR_FMT_DELAY("[%s]获取%s客户端连接信息失败: %s", uuidStr.c_str(), protocolType.c_str(), e.what());
    }

    LOG_INFO_FMT_DELAY("[%s]从%s端口[%d]连接成功: %s:%hu", uuidStr.c_str(), protocolType.c_str(), port, clientIp.c_str(), clientPort);
}

// 关闭所有acceptor，释放网络资源
// 核心逻辑：遍历所有 Acceptor 并关闭，避免端口占用等资源泄漏问题；
// 容错处理：捕获关闭时的错误并打印日志，不影响程序退出。
CServer::~CServer()
{
    // 关闭所有acceptor，释放网络资源
    for (auto& acceptor : _acceptors) {
        boost::system::error_code error;
        acceptor.close(error);
        if (error) {
            LOG_ERROR_FMT_DELAY("关闭Acceptor失败：%s", error.message().c_str());
        }
    }
    AsioIOServicePool::getInstance().stop();
    LOG_INFO_DELAY("CServer析构完成，所有网络资源已释放");
}