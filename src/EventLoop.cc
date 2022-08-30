#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <functional>
#include <memory>

/**
 * t_loopInThisThread是全局的EventLoop类型的指针变量，指向当前线程中的EventLoop对象，
 * t_loopInThisThread为nullptr时才会创建EventLoop对象，防止一个线程创建多个EventLoop
 * __thread就是thread_local，每个线程会拷贝一份该全局变量在自己的线程空间，每个线程一份
 */
 
__thread EventLoop* t_loopInThisThread = nullptr;


// 定义默认Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;


// 创建wakefd，用来notify唤醒subReactor，然后处理新的Channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0){
        // eventfd失败，会导致事件发生后无法通知subReactor（Ev entLoop）处理事件
        LOG_FATAL("Failed in eventfd! errno : %d\n", errno);
    }
    return evtfd;
}


EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n". this, threadId_);
    if(t_loopInThisThread != nullptr){
        LOG_FATAL("There has existed an EventLoop %p in this thread %d \n", this, threadId_);
    }else{
        // 当前线程第一次创建EventLoop对象
        t_loopInThisThread = this;
    }

    // 设置wakeupFd_的事件类型以及发生事件后的回调操作
    // EventLoop::handleRead方法只有一个参数，是EventLoop对象，由于我们传入了this，bind返回的是一个function<void()>的对象
    // 这里有一个疑问，function<void()>对象作为实参，传递给setReadCallBack形参ReadEventCallBack function<void(Timestamp)> ？？？
    wakeupChannel_->setReadCallBack(std::bind(&EventLoop::handleRead, this));
    // 每一个EventLoop都将监听wakeupChannel_的读事件，kReadEvent = EPOLLIN | EPOLLPRI
    wakeupChannel_->enableReading();
}


EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();    // 所有事件都不感兴趣 
    wakeupChannel_->remove();        // 
    ::close(wakeupChannel_->fd());   // 关闭fd
    t_loopInThisThread = nullptr;    // 指向线程中EventLoop对象的指针置空
}


// 开启事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_){
        activateChannels_.clear();
        // 当epoll_wait发生事件以后，poller会把发生事件的channel写入EventLoop的成员变量activateChannels_
        // 然后EventLoop就会调用发生事件Channel的回调函数
        // 监听两类回调函数：client fd和wakeupfd（用于mainReactor和subReactor通信）
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activateChannels_);
        // for循环处理所有发生事件的channel
        for(Channel* channel : activateChannels_){
            // poller监听哪些channel发生事件，然后上报给EventLoop，EventLoop通知channel调用相应的回调函数处理相应的事件（Channel执行回调）
            channel->handleEvent(pollReturnTime_);
        }
        // subReactor给唤醒后，执行需要处理的回调操作（mainReactor事先注册的），接收新用户的Channel
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop loop \n", this);
    looping_ = false;
}


// 退出事件循环
// 分两种情况：1.EventLoop所属线程调用quit    2.非EventLoop所属线程调用quit，这两种情况都是允许发生的
void EventLoop::quit(){
    // 把EventLoop的quit_置为true，表示要停止当前loop，EventLoop所属线程在loop函数中退出while循环
    quit_ = true;
    if(!isInLoopThread()){
        // 当前执行线程并不是EventLoop所属线程，则通过eventFd唤醒EventLoop所属线程，让其停止阻塞继续执行，接着就退出while循环
        // 如果EventLoop所属线程阻塞了，不进行唤醒的话，就算把quit_置为true，EventLoop所属线程也不会立刻退出，因为无法立即执行while的判断语句
        wakeup();
    }
}


// 在当前loop中执行回调cb
void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){
        cb();  // 在当前loop线程中
    }else{
        queueInLoop(cb);
    }
}


// queueInLoop是给非EventLoop所在线程执行的，把cb放到队列中，等到loop所在线程被唤醒后再执行cb
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);  // emplace_back是直接构造，push_back是拷贝构造
    }
    // 执行线程并非loop所在线程 || loop所在线程正在执行回调函数，但是loop又有了新的回调
    // loop执行完回调函数后会重新阻塞，所以需要通知，继续执行新来的回调
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup(); // 通知loop所在线程
    }
}


// wakeup方法是给其他loop所在线程执行的，用于唤醒loop所在线程，向该loop的wakeupFd_写数据即可，
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        // 真正写入的数据大小n并不是我们想写的sizeof(one)
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
    }
}


// Channel调用所在loop的updateChannel来epoll_ctl  EPOLL_CTL_ADD  EPOLL_CTL_MOD，修改Poller
void EventLoop::updateChannel(Channel* channel){
    poller_->updateChannel(channel);
}


// Channel调用所在loop的removeChannel来epoll_ctl  EPOLL_CTL_DEL
void EventLoop::removeChannel(Channel* channel){
    poller_->removeChannel(channel);
}


// 查看当前循环是否有管理参数传入的channel
bool EventLoop::hasChannel(Channel* channel){
    return poller_->hasChannel(channel);
}


/**
 * 当有新的客户端连接时，mainReactor会发送8字节的数据到wakeupFd_，
 * 发送的内容不重要，重要的是subReactor会监听wakeupFd_，一旦wakeupFd_有事件发生，subReactor就会被唤醒，
 * subReactor被唤醒后就能拿到新用户到来的Channel
 */
void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if(n != sizeof(one)){
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8 \n", n);
    }
}

// 执行回调。回调都在vector pendingFunctors_里面
void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor& functor : functors){
        functor();  // 执行当前loop需要执行的回调函数
    }

    callingPendingFunctors_ = false;
}
