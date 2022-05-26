#include "EventLoopThread.h"

#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{}

EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if(loop_ != nullptr){
        loop_->quit();    // 退出事件循环
        thread_.join();   // 等待子线程结束
    }
}

EventLoop* EventLoopThread::startLoop(){
    thread_.start();  // 底层创建一个新线程，并执行成员变量thread构造时传入的threadFunc

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr){
            cond_.wait(lock);   // 成员变量loop_没有被新线程初始化的时候，一直wait在lock上
        }
        loop = loop_;
    }
    return loop;
}

// threadFunc是在单独的新线程里面运行的
void EventLoopThread::threadFunc(){
    // 创建一个独立的eventloop，和上面的线程一一对应，one loop per thread
    EventLoop loop;
    if(callback_){
        // 如果我们实现传递了callback_，ThreadInitCallback就是在底层启一个新线程绑定EventLoop时调用的，进行一些init相关操作
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();  // 通知
    }

    loop.loop();     // EventLoop loop  => Poller.poll，开启事件循环，监听新用户的连接或者已连接用户的读写事件

    // 一般来说，loop是一直执行的，能执行到下面的语句，说明程序要退出了，要关闭事件循环
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
