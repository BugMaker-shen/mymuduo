#include <stdlib.h>

#include "Poller.h"
#include "EPollPoller.h"

Poller* Poller::newDefaultPoller(EventLoop* loop){
    if(std::getenv("MUDUO_USE_POLL")){
        // new poll的实例
        return nullptr;
    }else{
        return new EPollPoller(loop);
    }
}
