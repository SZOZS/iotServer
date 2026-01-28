#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <chrono>              // 超时控制（C++14 std::chrono::seconds）
#include <condition_variable>  // 线程同步（C++11/14核心）
#include <csignal>             // 信号处理（SIGINT/SIGTERM定义）
#include <cstdio>              // 字符串格式化（替代宏多参数问题）
#include <mutex>               // 互斥锁（C++14支持std::lock_guard优化）
#include <stdexcept>           // 标准异常（C++14完善的异常体系）

#include "configs/ConfigGlobal.h"        // 全局配置（端口/线程池大小）
#include "configs/ProtocolConfig.h"      // IOT协议配置
#include "logs/Logger.h"                 // 日志系统（单例）
#include "networks/AsioIOServicePool.h"  // Asio线程池（单例）
#include "networks/CServer.h"            // TCP服务器核心
#include "utils/FileManager.h"           // 文件工具（日志目录/路径）

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

int main()
{
    try {
        // ========== 1. 基础初始化 ==========
        LOG_INFO_DELAY("程序启动...");
        // 加载全局配置（端口、协议映射等）
        ConfigGlobal::getInstance();
        LOG_INFO_DELAY("配置加载[完成]...");
        // 初始化日志系统
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);
        Logger::getInstance().setLogTarget(LogTarget::BOTH);
        std::string globalLogPath = utils::FileManager::getDateDirectory() + "/global-app.log";
        utils::FileManager::createDirectory(utils::FileManager::getDateDirectory());
        Logger::getInstance().setLogFile(globalLogPath);
        LOG_INFO_DELAY("日志加载[完成]...");
        // 加载IOT协议配置（帧解析规则等）
        ProtocolConfigManager::getInstance().loadConfigs();
        LOG_INFO_DELAY("IOT协议加载[完成]...");

        // ========== 2. 线程池初始化 ==========
        auto& pool = AsioIOServicePool::getInstance();
        // 启动线程池（必须在CServer创建前执行）
        if (!pool.isRunning()) {
            pool.start();  // 注意：和CServer中的接口名保持一致（小写start）
            LOG_INFO_DELAY("Asio线程池启动[完成]...");
        } else {
            LOG_INFO_DELAY("Asio线程池已运行...");
        }

        // ========== 3. IO上下文+信号处理 ==========
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &pool](const boost::system::error_code& error, int signal_num) {
            if (error) {
                LOG_ERROR_FMT_DELAY("信号处理异常：%s", error.message().c_str());
                return;
            }
            LOG_WARNING_FMT_DELAY("收到终止信号[%d]，停止中...", signal_num);
            // 第一步：停止IO上下文（终止监听/异步操作）
            io_context.stop();
            LOG_INFO_DELAY("IO上下文已停止...");
            // 第二步：停止线程池（释放业务处理线程）
            pool.stop();  // 修正：和CServer中的接口名统一（小写stop）
            LOG_INFO_DELAY("Asio线程池已停止...");
            // 通知退出条件变量（可选）
            std::lock_guard<std::mutex> lock(mutex_quit);
            bstop = true;
            cond_quit.notify_all();
        });

        // ========== 4. 启动服务器 ==========
        LOG_INFO_DELAY("启动服务器，初始化端口监听...");
        CServer s(io_context);  // 创建CServer实例（内部会初始化Acceptor+协议映射）
        LOG_INFO_DELAY("服务器初始化完成，进入事件循环...");
        io_context.run();  // 启动IO上下文事件循环（阻塞，直到收到终止信号调用stop()）

        // ========== 5. 优雅退出收尾 ==========
        LOG_INFO_DELAY("服务器事件循环已退出...");
        // 等待所有资源释放（可选，根据实际业务调整）
        std::unique_lock<std::mutex> lock(mutex_quit);
        cond_quit.wait(lock, []() { return bstop; });
        LOG_INFO_DELAY("程序已正常停止，所有资源已释放");
    } catch (const std::exception& e) {
        LOG_ERROR_FMT_DELAY("程序异常退出：%s", e.what());
        return -1;
    } catch (...) {
        LOG_ERROR_DELAY("程序未知异常退出");
        return -2;
    }
    return 0;
}
