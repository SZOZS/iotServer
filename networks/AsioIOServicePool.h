#pragma once
#ifndef ASIO_IO_SERVICE_POOL_H
#define ASIO_IO_SERVICE_POOL_H

#include <boost/asio.hpp>
#include <vector>

#include "../patterns/Singleton.h"

// IO服务池：管理多个io_context和工作线程，实现负载均衡的异步IO处理
// 继承Singleton，确保全局唯一实例
class AsioIOServicePool : public Singleton<AsioIOServicePool>
{
public:
    // IO上下文类型（Boost.Asio核心）
    using IOService = boost::asio::io_context;
    // 工作守卫：防止io_context的run()方法在无任务时退出（Boost 1.89+版本使用executor_work_guard）
    using Work = boost::asio::executor_work_guard<IOService::executor_type>;
    // 工作守卫的智能指针
    using WorkPtr = std::unique_ptr<Work>;
    // 析构函数：停止服务和线程
    ~AsioIOServicePool();
    // 禁用拷贝构造
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    // 禁用赋值运算符
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;
    // 获取一个io_context（轮询方式，均衡分配任务）
    boost::asio::io_context& getIOService();
    // 停止所有io_context和工作线程
    void Stop();

private:
    // 构造函数（私有）：初始化服务池
    // 参数：size - 服务池大小（默认为CPU核心数，最大化利用硬件资源）
    // AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
    AsioIOServicePool(std::size_t size = 4);
    // 必须声明父类为友元，否则Singleton<T>::getInstance()无法创建实例
    friend Singleton<AsioIOServicePool>;
    // 存储多个io_context实例
    std::vector<IOService> _ioServices;
    // 每个io_context对应的工作守卫（保持run()不退出）
    std::vector<WorkPtr> _works;
    // 每个io_context对应的工作线程
    std::vector<std::thread> _threads;
    // 轮询索引：记录下一个要分配的io_context位置
    std::size_t _nextIOService;
};

#endif  // ASIO_IO_SERVICE_POOL_H