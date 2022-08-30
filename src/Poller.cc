#include "Poller.h"
#include "Channel.h"

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const{
    auto iter = channels_.find(channel->fd());
    return iter != channels_.end() && iter->second == channel;  // 找到fd且channel相等
}
