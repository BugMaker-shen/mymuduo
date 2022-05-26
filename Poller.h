#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;  //只用到指针类型，如果需要用到实例就要包含相应头文件，类型声明是没用的
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable{
    public:
        using ChannelList = std::vector<Channel*>;

        Poller(EventLoop *loop);
        virtual ~Poller();
 
        // 给所有IO复用保留统一的接口  epoll_wait
        virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
        // 更新感兴趣的事件  epoll_ctl  EPOLL_CTL_ADD  EPOLL_CTL_MOD
        virtual void updateChannel(Channel* channel) = 0;
        // eventloop中删除channel  epoll_ctl  EPOLL_CTL_DEL
        virtual void removeChannel(Channel* channel) = 0;
        // 判断Poller里是否包含某个channel
        bool hasChannel(Channel* channel) const;

        // EventLoop可以通过newDefaultPoller获取一个Poller实例
        static Poller* newDefaultPoller(EventLoop* loop);

    protected:
        // key:sockfd，value:sockfd所属的Channel
        using ChannelMap = std::unordered_map<int, Channel*>;
        ChannelMap channels_;

    private:
        EventLoop* ownerLoop_;  // Poller所属的事件循环
};
