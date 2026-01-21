#include <iostream>

#include "../logs/Logger.h"

#include "AsioIOServicePool.h"

// 构造函数：初始化io_context、工作守卫和线程
AsioIOServicePool::AsioIOServicePool(std::size_t size) : _ioServices(size), _works(size), _nextIOService(0)
{
    // 为每个io_context创建工作守卫（防止run()退出）
    for (std::size_t i = 0; i < size; ++i) {
        _works.emplace_back(std::make_unique<Work>(boost::asio::make_work_guard(_ioServices[i])));
    }
    // 为每个io_context启动一个线程，线程中运行io_context的事件循环
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() { _ioServices[i].run(); });
    }
    LOG_INFO_FMT("初始化[%d]个线程", std::thread::hardware_concurrency());
}

// 析构函数：停止服务池
AsioIOServicePool::~AsioIOServicePool()
{
    // 调用Stop释放资源
    Stop();
    LOG_WARNING("已销毁");
}

// 轮询获取io_context（负载均衡）
boost::asio::io_context& AsioIOServicePool::getIOService()
{
    // 获取当前索引的io_context
    auto& service = _ioServices[_nextIOService++];
    // 索引循环（超过大小后重置为0）
    if (_nextIOService == _ioServices.size()) {
        _nextIOService = 0;
        LOG_INFO("索引已重置");
    }
    return service;
}

// 停止所有io_context和线程
void AsioIOServicePool::Stop()
{
    // 1. 重置工作守卫（让io_context的run()在处理完当前任务后退出）
    for (auto& work : _works) {
        // 释放工作守卫，io_context将在无任务时退出run()
        work.reset();
        LOG_INFO("工作守卫已重置");
    }
    // 2. 主动停止所有io_context（确保快速退出）
    for (auto& io : _ioServices) {
        io.stop();
        LOG_WARNING("IO上下文已停止");
    }
    // 3. 等待所有线程结束
    for (auto& t : _threads) {
        // 若线程可join，等待其退出
        if (t.joinable()) {
            t.join();
            LOG_WARNING("线程等待退出中...");
        }
        LOG_WARNING("线程已退出");
    }
}