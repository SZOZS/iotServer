#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>

#include "configs/ConfigLog.h"
#include "configs/ProtocolConfig.h"
#include "frames/FrameParserDispatcher.h"
#include "frames/sd/rtu/RTUParser.h"
#include "logics/LogicSystem.h"
#include "logs/Logger.h"
#include "networks/AsioIOServicePool.h"
#include "networks/CServer.h"
#include "patterns/Singleton.h"
#include "utils/FileManager.h"

bool bstop = false;
std::condition_variable cond_quit;
std::mutex mutex_quit;

int main()
{
    try
    {
        LOG_INFO("程序启动...");
        // 加载配置
        ConfigGlobal::getInstance();
        LOG_INFO("配置加载[完成]...");

        // 日志
        Logger::getInstance().setLogLevel(LogLevel::DEBUG);
        Logger::getInstance().setLogTarget(LogTarget::BOTH);
        std::string globalLogPath = utils::FileManager::getDateDirectory() + "/global-app.log";
        utils::FileManager::createDirectory(utils::FileManager::getDateDirectory());
        Logger::getInstance().setLogFile(globalLogPath);
        LOG_INFO("日志加载[完成]...");

        // IOT协议
        ProtocolConfigManager::getInstance().loadConfigs();
        LOG_INFO("IOT协议加载[完成]...");

        // 线程池
        auto &pool = AsioIOServicePool::getInstance();
        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &pool](auto, auto)
                           {
            LOG_WARNING("收到终止信号，停止中...");
            io_context.stop();
            pool.Stop(); });

        // ConfigGlobal::logPortProtocolConfig();  // 打印端口协议配置
        // std::vector<short> ports = ConfigGlobal::getUniquePortsFromConfig();
        // CServer s(io_context, ports);
        CServer s(io_context);
        io_context.run();
        LOG_INFO("程序已停止");
    }
    catch (std::exception &e)
    {
        LOG_ERROR(e.what());
    }
}
