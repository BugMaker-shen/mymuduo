#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;

/**
 * 事件循环类：主要包含两个大模块 Channel（包含了监听的sockfd，感兴趣的事件和发生的事件） Poller（epoll的抽象）
 */ 
class EventLoop : noncopyable{
    public:
        using Functor = std::function<void()>;

        EventLoop();
        ~EventLoop();

        void loop();                                 // 开启事件循环
        void quit();                                 // 退出事件循环

        Timestamp pollReturnTime() const { return pollReturnTime_; }

        void runInLoop(Functor cb);                  // 在当前loop中执行回调cb
        void queueInLoop(Functor cb);                // 把cb放到队列中，等到loop所在线程被唤醒后再执行cb

        void wakeup();                               // 用于唤醒loop所在线程

        void updateChannel(Channel* channel);        // Channel调用所在loop的updateChannel来epoll_ctl  EPOLL_CTL_ADD  EPOLL_CTL_MOD
        void removeChannel(Channel* channel);        // Channel调用所在loop的removeChannel来epoll_ctl  EPOLL_CTL_DEL

        bool hasChannel(Channel* channel);           // 查看当前循环是否有管理参数传入的channel

        bool isInLoopThread() const {
            // threadId_是loop所在线程缓存的tid，CurrentThread::tid()返回的是执行线程的tid
            // 如果不相等，那得等到当前loop所属线程被唤醒的时候，才能执行loop相关的回调操作
            return threadId_ == CurrentThread::tid();
        }

    private:
        void handleRead();                           // 主要用于wakeup
        void doPendingFunctors();                    // 执行回调。回调都在vector pendingFunctors_里面

        using ChannelList = std::vector<Channel*>;

        std::atomic_bool looping_;                   // 是否正常事件循环，原子操作，通过CAS实现
        std::atomic_bool quit_;                      // 退出loop循环的标志
        
        const pid_t threadId_;                       // 记录创建EventLoop对象的线程tid，会用于和执行的线程id比较，就可以判断这个Eventloop对象在不在当前线程里
        
        Timestamp pollReturnTime_;                   // 记录Poller返回发生事件的Channels的时间，有事件发生后，Poller会返回所有有事件发生的Channel
        std::unique_ptr<Poller> poller_;             // EventLoop对象管理的唯一的poller

        int wakeupFd_;                               // eventfd()创建的，作用是当mainLoop获取一个新用户的Channel，通过轮询算法选择一个subLoop，通过wakeupFd_唤醒subLoop处理事件，每一个subReactor都监听了wakeupFd_
        std::unique_ptr<Channel> wakeupChannel_;     // 用于封装wakeupFd_

        ChannelList activateChannels_;               // EventLoop中有事件发生的channel

        std::atomic_bool callingPendingFunctors_;    // 标识当前loop是否正在执行的回调操作
        std::vector<Functor> pendingFunctors_;       // 存放loop具体处理事件的回调操作
        std::mutex mutex_;                           // 用于保护pendingFunctors_的线程安全
};
