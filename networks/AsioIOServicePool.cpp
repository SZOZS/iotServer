#include <iostream>

#include "../logs/Logger.h"

#include "AsioIOServicePool.h"

// 构造函数：初始化io_context、工作守卫和线程
AsioIOServicePool::AsioIOServicePool(std::size_t size) : _ioServices(size), _nextIOService(0)
{
    if (size == 0) {
        throw std::invalid_argument("IO服务池大小不能为0");
    }
    // 为每个io_context创建工作守卫（防止run()无任务时退出）
    _works.reserve(size);  // 预分配空间，避免扩容
    for (std::size_t i = 0; i < size; ++i) {
        _works.emplace_back(std::make_unique<Work>(boost::asio::make_work_guard(_ioServices[i])));
    }
    // 为每个io_context启动工作线程，运行事件循环
    _threads.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        _threads.emplace_back([this, i]() {
            try {
                _ioServices[i].run();  // 运行IO上下文事件循环
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("IO上下文[%zu]运行异常：%s", i, e.what());
            }
        });
    }
    LOG_INFO_FMT("IO服务池初始化完成，线程数：%zu（CPU核心数：%u）", size, std::thread::hardware_concurrency());
}

// 析构函数：停止服务池
AsioIOServicePool::~AsioIOServicePool()
{
    // 调用Stop释放资源
    stop();
    LOG_WARNING("已销毁");
}

// 轮询获取io_context（负载均衡）
boost::asio::io_context& AsioIOServicePool::getIOService()
{
    // 获取当前索引的io_context
    std::size_t idx = _nextIOService.fetch_add(1, std::memory_order_relaxed);
    idx %= _ioServices.size();  // 取模实现循环轮询（比重置0更高效）

    // 可选：仅在调试模式打印索引（避免高频日志）
    LOG_DEBUG_FMT("分配IO上下文索引：%zu", idx);
    return _ioServices[idx];

    // auto& service = _ioServices[_nextIOService++];
    // // 索引循环（超过大小后重置为0）
    // if (_nextIOService == _ioServices.size()) {
    //     _nextIOService = 0;
    //     LOG_INFO("索引已重置");
    // }
    // return service;
}

// 停止所有io_context和线程
void AsioIOServicePool::stop()
{
    if (_ioServices.empty()) {
        return;  // 避免重复停止
    }
    LOG_WARNING("开始停止IO服务池...");

    // 步骤1：主动停止所有IO上下文（快速终止未处理的异步任务）
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _ioServices[i].stop();
        LOG_DEBUG_FMT("IO上下文[%zu]已主动停止", i);
    }

    // 步骤2：释放工作守卫（让IO上下文的run()彻底退出）
    _works.clear();  // 批量释放比逐个reset更高效
    LOG_INFO("所有工作守卫已释放");

    // 步骤3：等待所有工作线程退出
    for (std::size_t i = 0; i < _threads.size(); ++i) {
        if (_threads[i].joinable()) {
            _threads[i].join();
            LOG_DEBUG_FMT("工作线程[%zu]已退出", i);
        }
    }
    _threads.clear();     // 清空线程容器，释放资源
    _ioServices.clear();  // 清空IO上下文容器

    LOG_WARNING("IO服务池已完全停止");

    // // 1. 重置工作守卫（让io_context的run()在处理完当前任务后退出）
    // for (auto& work : _works) {
    //     // 释放工作守卫，io_context将在无任务时退出run()
    //     work.reset();
    //     LOG_INFO("工作守卫已重置");
    // }
    // // 2. 主动停止所有io_context（确保快速退出）
    // for (auto& io : _ioServices) {
    //     io.stop();
    //     LOG_WARNING("IO上下文已停止");
    // }
    // // 3. 等待所有线程结束
    // for (auto& t : _threads) {
    //     // 若线程可join，等待其退出
    //     if (t.joinable()) {
    //         t.join();
    //         LOG_WARNING("线程等待退出中...");
    //     }
    //     LOG_WARNING("线程已退出");
    // }
}

bool AsioIOServicePool::isRunning() const
{
    return !_ioServices.empty() && !_threads.empty();
}
void AsioIOServicePool::start()
{
    if (isRunning())
        return;
    // 若需要手动启动，补充启动逻辑（建议构造时自动启动）
}