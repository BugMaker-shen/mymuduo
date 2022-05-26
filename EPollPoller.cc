#include "EPollPoller.h"
#include "Channel.h"
#include "Logger.h"
#include "Timestamp.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>

// 标识channel的状态 
const int kNew = -1;      // Channel从来没有添加到epoll或者从epoll和Poller中的map都删除    Channel的成员index_初始化-1
const int kAdded = 1;     // Channel已经添加到了epoll和Poller中的map
const int kDeleted = 2;   // Channel从epoll红黑树中删除了，但是还存在于Poller的map中

// 构造函数，调用了epoll_create
EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop)
    , epollfd_(epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)  // vector<epoll_event>
{
    if(epollfd_ < 0){
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller(){
    // epoll结束的时候，关闭内核事件表fd
    ::close(epollfd_);
}

// EventLoop创建ChannelList，并把ChannelList传到EPollPoller::poll，poll会把发生事件的channel（即活跃的）通过activeChannels填到EventLoop的成员变量ChannelList中
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels){
    // 实际上应该用LOG_DEBUG更合适
    LOG_INFO("func=%s => fd total count : %lu \n", __FUNCTION__, channels_.size());
    // events_.begin()返回首元素的迭代器，也就是首元素的地址，是面向对象的，解引用后就是首元素的值，然后取地址就得到了vector封装的底层数组的首地址
    // epoll_wait返回后，events_底层的数组的前numEvents元素就是所有发生事件的epoll_event
    int numEvents = ::epoll_wait(epollfd_, &(*events_.begin()), static_cast<int>(events_.size()), timeoutMs);
    // 全局变量errno，poll可能在多个线程中的eventloop被调用，被读写，所以先用局部变量存起来
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0){
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);  // 这里每次传入的都是一个空vector，把发生事件的Channel添加到EventLoop的成员变量ChannelList中
        if(numEvents == (int)events_.size()){
            // 发生事件的fd和成员变量EventList的长度相同，需要扩容
            events_.resize(events_.size() * 2);
        }
    }else if(numEvents == 0){
        // 没有事件发生，超时
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }else{
        if(saveErrno != EINTR){
            // 不等于外部的中断，是由其他错误类型引起的
            errno = saveErrno;  // 把全局变量errno重新置为当前错误saveErrno
            LOG_ERROR("EPollPoller::poll error!");
        }
    }
    return now;
}

// Channel调用update remove ==> EventLoop updateChannel removeChannel ==> Poller updateChannel removeChannel
// epoll_ctl  EPOLL_CTL_ADD  EPOLL_CTL_MOD
void EPollPoller::updateChannel(Channel* channel){
    const int index = channel->index();  
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if(index == kNew || index == kDeleted){
        if(index == kNew){
            // 如果从未添加到EPollPoller中，那么将fd和对应channel对象注册到Poller中的成员map中
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel); // epoll_ctl EPOLL_CTL_ADD
    }else{
        // index == kAdded
        int fd = channel->fd();
        if(channel->isNoneEvent()){
            // 对任何事件都不感兴趣，从epoll红黑树中删除
            channel->set_index(kDeleted);
            update(EPOLL_CTL_DEL, channel);
        }else{
            // 对某些事件感兴趣
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// eventloop中删除channel(fd)  epoll_ctl  EPOLL_CTL_DEL
void EPollPoller::removeChannel(Channel* channel){
    int fd = channel->fd();
    channels_.erase(fd);   // 先在Poller里的map中删除

    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel); // 从epoll红黑树中删除
    }
    channel->set_index(kNew);  // 最终的结果就是要使得Channel的状态是kNew
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const{
    for(int i = 0; i < numEvents; ++i){
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);     // EventLoop就拿到了它的Poller返回的所有发生事件的Channel列表了
    }
}

// epoll_ctl  EPOLL_CTL_ADD，EPOLL_CTL_DEL，EPOLL_CTL_MOD  更新channel
void EPollPoller::update(int operation, Channel* channel){
    epoll_event event;
    memset(&event,  0, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        // epoll_ctl返回值小于0，则出错
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl delete error:%d\n", errno);
        }else{
            LOG_FATAL("epoll_ctl add / modify error:%d\n", errno);
        }
    }
}