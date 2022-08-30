#pragma once

#include "Poller.h"
#include "Timestamp.h"
 
#include <vector>
#include <sys/epoll.h>

class Channel;
 
// 继承Poller的时候需要知道Poller的实际结构，而不仅仅是一个类型声明
class EPollPoller : public Poller{
    public:
        EPollPoller(EventLoop* loop);
        ~EPollPoller() override;

        // epoll_wait
        Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
        // epoll_ctl 更新感兴趣的事件 EPOLL_CTL_ADD  EPOLL_CTL_MOD  
        void updateChannel(Channel* channel) override;
        // eventloop中删除channel(fd)  EPOLL_CTL_DEL
        void removeChannel(Channel* channel) override;

    private:
        static const int kInitEventListSize = 16;

        // 填写活跃的连接，被poll方法调用
        void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
        // 更新channel，被updateChannel()和removeChannel()调用
        void update(int operation, Channel* channel);

        using EventList = std::vector<epoll_event>;

        int epollfd_;       // epoll_create创建，对应内核时间表
        EventList events_;  // 存放epoll_event事件的vector
};
