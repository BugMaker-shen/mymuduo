#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    ,tied_(false)
{}

Channel::~Channel(){}

//TcpConnection::connectEstablished会调用tie，传TcpConnection对象的shared_ptr给obj
void Channel::tie(const std::shared_ptr<void>& obj){
    //  weak_ptr tie_ 观察 shared_ptr obj
    tie_ = obj;
    tied_ = true;
}

/*
  向Poller（poll/epoll）更新fd感兴趣的事件，就是epoll_ctl
  EventLoop包含Channel、Poller，在Channel中的update方法需要更新epoll内核事件表中fd感兴趣的事件，
  然而epoll内核事件表需要由Poller操作，所以Channel通过所属的EventLoop更新epoll内核事件表，其实EventLoop也是通过Poller类更新的EventLoop
*/
void Channel::update(){
    // 通过Channel所属的EventLoop，调用Poller的方法，注册fd的events事件
    loop_->updateChannel(this);
}

void Channel::remove(){
    // 在当前channel所属的eventloop中删除当前channel
    loop_->removeChannel(this);
}

// 得到poller通知以后，处理事件
void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGaurd(receiveTime);
        }
    }else{
        handleEventWithGaurd(receiveTime);
    }
}

// 根据接收到的事件，执行对象的回调函数处理
void Channel::handleEventWithGaurd(Timestamp receiveTime){
    // LOG_INFO("in function : %s\n", __FUNCTION__);
    // LOG_INFO("line : %s\n", __LINE__);

    LOG_INFO("channel handleEvent revents: %d\n", revents_);

    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        // 读写都关闭 && 没有读事件，表示出问题了
        if(closeCallBack_){
            // 回调函数对象不空，执行回调
            closeCallBack_(); 
        }
    }

    if(revents_ & EPOLLERR){
        if(errorCallBack_){
            errorCallBack_(); 
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)){
        if(readCallBack_){
            readCallBack_(receiveTime); 
        }
    }

    if(revents_ & EPOLLOUT){
        if(writeCallBack_){
            writeCallBack_(); 
        }
    }
}