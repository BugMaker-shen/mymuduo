#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int32_t Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))         // 把func底层的资源给func_，效率更高
    , name_(name)
{
    // 成员变量thread_采用默认构造
    setDefaultName();                // 给线程一个默认的名字
}

Thread::~Thread(){
    // joined_表示是一个工作线程，状态会受到主线程状态的一影响，而守护线程拥有自动结束自己生命周期的特性，而非守护线程不具备这个特点。
    if(started_ && !joined_){
        // 析构的时候，线程启动了才会有相应的操作
        thread_->detach();           // std::thread类提供的设置分离线程的方法，detach后成为守护线程，守护线程结束后，内核自动回收，不会出现孤儿线程
    }
}

// 线程启动，这里涉及两个线程，线程A调用start并创建新的线程B，假如调用start的线程A走得快，很快就执行到了函数最后，如果新的执行func_()的线程B还没有创建出来，线程A就会阻塞在sem_wait，等待子线程B创建
// 这样的话，如果一旦有一个线程调用了start，那就可以放心的使用这个线程的tid，因为此时这个线程是肯定存在的
void Thread::start(){
    // 一个Thread对象记录的就是一个新线程的详细信息
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // std::thread的构造函数需要一个函数对象，可以使用lambda表达式，以引用形式接收外部参数
    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);    // 信号量的V操作
        func_();           // EventLoopThread构造函数中传入的EventLoopThread::threadFunc
    }));

    // 这里必须等待获取上面新创建线程的tid，用来阻塞当前线程直到信号量sem的值大于0，sem_wait相当于P操作，等线程创建后获取tid，sem的值才能+1
    sem_wait(&sem);
}

void Thread::join(){
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if(name_.empty()){
        char buff[32] = {0};
        snprintf(buff, sizeof(buff), "Thread%d", num);
        name_ = buff;
    }
}
