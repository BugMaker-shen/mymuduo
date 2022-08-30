#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h>
#include <string>
#include <atomic>

class Thread : noncopyable{
    public:
        using ThreadFunc = std::function<void()>;  // 没有参数，如果想传参，可以使用绑定器
        explicit Thread(ThreadFunc, const std::string& name = std::string());
        ~Thread();

        void start();
        void join();

        bool started() const { return started_; }
        pid_t tid() const { return tid_; }         // muduo返回的线程tid相当于使用top命令查看的线程tid，不是pthread_self打印出来的真实的线程号
        const std::string& name() const { return name_; }
        static int numCreated() { return numCreated_; }

    private:
        void setDefaultName();

        bool started_;                            // 当前线程是否启动 
        bool joined_;                             // 当前线程等待其他线程运行完，当前线程继续运行
        std::shared_ptr<std::thread> thread_;     // 自己控制线程的启动
        pid_t tid_;
        ThreadFunc func_;                         // 线程函数
        std::string name_;                        // 线程名字，调试时可使用
        static std::atomic_int32_t numCreated_;   // 创建的线程数
};
