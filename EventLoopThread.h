#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

class EventLoopThread : noncopyable{
    public:
        using ThreadInitCallback = std::function<void(EventLoop*)>;
        EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string());
        ~EventLoopThread();
        
        EventLoop* startLoop();

    private:
        void threadFunc();

        EventLoop* loop_;
        bool exiting_;                 // 是否退出事件循环
        Thread thread_;
        std::mutex mutex_;
        std::condition_variable cond_;
        ThreadInitCallback callback_;  // 启一个新线程绑定EventLoop时调用的，进行一些init相关操作
};
