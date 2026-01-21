#pragma once
#ifndef SINGLETON_H
#define SINGLETON_H

#include <iostream>
#include <utility>  // 用于删除移动操作

// 单例模式模板类（C++14实现）
template <typename T>
class Singleton
{
protected:
    // 保护构造函数：允许子类继承，禁止外部直接实例化
    Singleton() = default;

    // 禁止拷贝构造和拷贝赋值
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>&) = delete;

    // 禁止移动构造和移动赋值（C++11起支持）
    Singleton(Singleton<T>&&) = delete;
    Singleton& operator=(Singleton<T>&&) = delete;

    // 虚析构函数：确保子类析构函数正确调用
    virtual ~Singleton() { std::cout << "单例模板析构函数被用于：" << typeid(T).name() << std::endl; }

public:
    // 全局访问点：返回单例实例的引用
    // C++11及以后标准保证局部静态变量初始化的线程安全性
    static T& getInstance()
    {
        static T instance;  // 首次调用时初始化，生命周期与程序一致
        return instance;
    }

    // 打印实例地址（用于调试，验证单例唯一性）
    void printAddress() const { std::cout << "单例实例地址：" << static_cast<const void*>(this) << std::endl; }
};

#endif  // SINGLETON_H