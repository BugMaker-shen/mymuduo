#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>

static int createNonblocking(){
    // 创建listenfd
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if(sockfd <= 0){
        LOG_FATAL("%s:%s:%d listen socket crete err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())             // Socket的构造函数需要一个int参数，createNonblocking()返回值就是int
    , acceptChannel_(loop, acceptSocket_.fd())       // 第一个参数就是Channel所属的EventLoop
    , listening_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);        // bind
    // 当我们TcpServer调用start方法时，就会启动Acceptor的listen方法
    // 有新用户连接时，要执行一个回调，这个方法会将和用户连接的fd打包成Channel，然后交给subloop
    // 下面就是注册包装了listenfd的Channel发生读事件后，需要执行的回调函数，，Acceptor只管理封装了listenfd的Channel，只需要注册读事件的回调
    acceptChannel_.setReadCallBack(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen(){
    listening_ = true;              // 开始监听
    acceptSocket_.listen();         // 调用底层的::listen
    acceptChannel_.enableReading(); // 将acceptChannel_注册到Poller，或者在Poller更新自己感兴趣的事件 
}

// 服务器的listenfd发生读事件调用，即新用户连接
void Acceptor::handleRead(){
    InetAddress peerAddr;  // 客户端地址
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        if(newConnetionCallback_){
            newConnetionCallback_(connfd, peerAddr);  // 轮询找到subloop，唤醒并分发封装connfd的Channel
        }else{
            // 如果没有设置新用户连接的回调操作，就关闭连接
            ::close(connfd);
        }
    }else{
        // accept出错
        LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE){
            // 打开的fd达到上限
            LOG_ERROR("%s:%s:%d the open fd exceeding the resource limit\n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
