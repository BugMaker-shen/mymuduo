#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable{
    public:
        using ThreadInitCallback = std::function<void(EventLoop*)>;

        EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
        ~EventLoopThreadPool();

        // 设置底层线程的数量，TcpServer::setThreadNum底层调用的就是EventLoopThreadPool::setThreadNum
        void setThreadNum(int numThreads){ numThreads_ = numThreads; }
        void start(const ThreadInitCallback& cb = ThreadInitCallback());

        // 如果工作在多线程中，baseLoop_默认以轮询的方式分配Channel给subLoop
        EventLoop* getNextLoop();
        // 返回事件循环池所有的EventLoop
        std::vector<EventLoop*> getAllLoops();

        bool started() const{ return started_; }
        const std::string name() const { return name_; }

    private:
        EventLoop* baseLoop_; // 我们使用muduo编写程序的时候，就会定义一个EventLoop变量，这个变量作为TcpServer构造函数的参数，用户创建的就叫做baseLoop
        std::string name_;    // 线程池的名字
        bool started_;
        int numThreads_;
        long unsigned int next_;
        std::vector<std::unique_ptr<EventLoopThread>> threads_;  // 包含了创建的所有subLoop的线程，和loops_一一对应
        std::vector<EventLoop*> loops_;                          // 包含了所有创建的subLoop的指针，这些EventLoop对象都是栈上的（见EventLoopThread::threadFunc）
};
