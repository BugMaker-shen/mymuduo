#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool(){}

void EventLoopThreadPool::start(const ThreadInitCallback& cb){
    started_ = true;
    // 用户没有调用EventLoopThreadPool::setThreadNum()
    for(int i = 0; i < numThreads_; ++i){
        char buff[name_.size() + 32];  // 用“线程池的名字 + 下标”作为底层线程的名字
        snprintf(buff, sizeof(buff), "%s%d", name_.c_str(), i);
        EventLoopThread* thread = new EventLoopThread(cb, buff);
        threads_.push_back(std::unique_ptr<EventLoopThread>(thread));  // 用unique_ptr管理堆上的EventLoopThread对象，以免我们手动释放
        loops_.push_back(thread->startLoop());                         // 调用EventLoopThread::startLoop后，会返回一个栈上的EventLoop对象，事件循环不停止，栈上的EventLoop对象不释放
    }

    // 如果用户没有调用EventLoopThreadPool::setThreadNum()，numThreads_默认为0
    if(numThreads_ == 0 && cb){
        // 整个服务器只有一个线程，即用户创建的baseLoop_
        cb(baseLoop_);
    }
}

// 如果工作在多线程中，baseLoop_默认以轮询的方式分配Channel给subLoop
EventLoop* EventLoopThreadPool::getNextLoop(){
    EventLoop* loop = baseLoop_;
    if(!loops_.empty()){
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size()){
            next_ = 0;
        }
    }
    return loop;
}

// 返回事件循环池所有的EventLoop
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty()){
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    return loops_;
}
