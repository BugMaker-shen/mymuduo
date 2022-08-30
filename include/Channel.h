#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;

/**
 *  Channel理解为通道，封装了sockfd和其感兴趣的事件，如EPOLLIN、EPOLLOUT事件，还包括Poller返回的具体事件
 */

class Channel : noncopyable{
    public:
        using EventCallBack =  std::function<void()>;
        using ReadEventCallBack = std::function<void(Timestamp)>;

        Channel(EventLoop* loop, int fd);         // loop是指针，编译阶段能确定大小，不需要知道EventLoop的定义，无需包含头文件
        ~Channel();
        
        // 得到poller通知以后，处理事件
        void handleEvent(Timestamp receiveTime);  // 由于receiveTime是对象，不是指针，编译阶段需要确定对象的大小，所以只有Timestamp的声明不够，需要有类定义，即需要包含头文件

        // 设置回调操作 不需要形参的局部对象，将左值转换成右值
        void setReadCallBack(ReadEventCallBack cb){ readCallBack_ = std::move(cb); }
        void setWriteCallBack(EventCallBack cb){ writeCallBack_ = std::move(cb); }
        void setCloseCallBack(EventCallBack cb){ closeCallBack_ = std::move(cb); }
        void setErrorCallBack(EventCallBack cb){ errorCallBack_ = std::move(cb); }

        // 防止channel被手动remove，channel还在执行回调操作
        void tie(const std::shared_ptr<void>&);
        int fd() const { return fd_; }

        int events() const{ return events_; }
        // 设置revents_，poller（poll/epoll）在监听fd，发生事件后调用set_revents告诉channel发生了什么事件
        void set_revents(int revt){ revents_ = revt; }
        
        void enableReading(){
            events_ |= kReadEvent;  // 相当于是添加感兴趣的事件，把events_中和read相关的位 置1
            update();               // epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &events_)
        }
        
        void disableReading(){
            events_ &= ~kReadEvent; // 删除读事件，取反后&，会把events_中和read相关的位 置0
            update();
        }

        void enableWriting(){ events_ |= kWriteEvent; update(); }
        void disableWriting(){ events_ &= ~kWriteEvent; update(); }
        void disableAll(){ events_ = kNoneEvent; update(); }

        bool isNoneEvent() const{ return events_ == kNoneEvent; }
        bool isWritingEvent() const{ return events_ & kWriteEvent; }
        bool isReadingEvent() const{ return events_ & kReadEvent; }

        int index(){ return index_; }
        void set_index(int idx){ index_ = idx; }

        // one thread per thread
        // 一个channel对应一个EventLoop，一个EventLoop对应多个channel，一个channel包含一个fd以及这个fd对应的感兴趣的事件以及发生的事件
        EventLoop* ownerLoop(){ return loop_; }

        // 用于删除channel
        void remove();

    private:
        void update();
        void handleEventWithGaurd(Timestamp receiceTime);

        // 表示当前fd的状态，对什么事件感兴趣
        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        EventLoop* loop_;  // 归属于哪个事件循环 epoll_wait
        const int fd_;     // Poller监听的对象
        int events_;       // 注册fd感兴趣的事件
        int revents_;      // Poller返回的具体发生的事件
        int index_;        // 初始化为-1，用于标识Channel的状态

        std::weak_ptr<void> tie_;  // 防止channel被手动remove后，我们还在使用channel
        bool tied_;

        // Poller通知Channel发生的事件会放在revents_，所以Channel负责调用具体事件的回调操作
        ReadEventCallBack readCallBack_;
        EventCallBack writeCallBack_;
        EventCallBack closeCallBack_;
        EventCallBack errorCallBack_;
};
