// 头文件编译期间都会在源文件中展开
#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

// static表示本文件可见
static EventLoop* CheckLoopNotNull(EventLoop* loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)                  // TcpConnection初始状态为正在连接
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))  // 构造Channel的时候就把Channel放入了一个subloop进行管理
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)         // 64M
{
    // 我们muduo用户把这些操作注册给TcpServer，TcpServer传递给TcpConnection，TcpConnection给Channel设置回调函数，这些方法都是Poller监听到事件后，Channel需要调用的函数
    channel_->setReadCallBack(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
    );
    channel_->setWriteCallBack(
        std::bind(&TcpConnection::handleWrite, this)
    );
    channel_->setCloseCallBack(
        std::bind(&TcpConnection::handleClose, this)
    );
    channel_->setErrorCallBack(
        std::bind(&TcpConnection::handleError, this)
    );

    LOG_INFO("TcpConnection::TcpConnection[%s] as fd=%d \n", name_.c_str(), sockfd);
    // 调用setsockopt启动socket的保活机制
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    // 析构函数不需要做什么，只有socket_、channel_是new出来的，这俩用智能指针管理会自动释放
    LOG_INFO("TcpConnection::~TcpConnection[%s] as fd=%d state=%d \n", name_.c_str(), socket_->fd(), (int)state_);
}

void TcpConnection::send(const std::string& buff){
    // 发送数据需要检查TcpConnection的状态
    if(state_ == kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buff.c_str(), buff.size());
        }else{
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buff.c_str(), buff.size())
            );
        }
    }
}

// 应用写的快，而内核发送数据快，需要把待发送数据写入outputBuffer_缓冲区，并设置了水位回调
void TcpConnection::sendInLoop(const void* data, size_t len){
    ssize_t nwriten = 0;              // 已发送的数据
    size_t remaining = len;          // 没发送的数据
    bool faultError = false;
    // 之前该TcpConnection对象调用过shutdown，不能再进行发送了
    if(state_ == kDisconnected){
        LOG_ERROR("TcpConnection::sendInLoop TcpConnection Disconnected, give up writing!\n");
        return;
    }
    if(!channel_->isWritingEvent() && outputBuffer_.readableBytes() == 0){
        // channel_第一次写数据时，对写事件不感兴趣开始写数据 && 缓冲区没有待发送的数据
        nwriten = ::write(channel_->fd(), data, len);
        if(nwriten >= 0){
            remaining = len - nwriten;
            if(remaining == 0 && writeCompleteCallback_){
                // 如果数据刚好发送完了 && 用户注册过发送完成的回调writeCompleteCallback_
                // 数据没有发送完时，channel_才对写事件感兴趣
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件，handleWrite也不会再执行了，handleWrite就是有epollout事件发生时执行
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this())
                );
            }
        }else{
            // nwriten < 0
            nwriten = 0;
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::sendInLoop \n");
                if(errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        }

        // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，
        // 然后给channel注册epollout事件，一旦poller发现tcp的发送缓冲区有空间，会通知相应的Channel，调用writeCallback_回调方法
        // Channel调用的writeCallback_就是TcpConnection注册的handleWrite，继续往TCP的发送缓冲区中写入outputBuffer_的数据
        if(!faultError && remaining > 0){
            // 目前outputBuffer_中积攒的待发送的数据
            size_t oldLen = outputBuffer_.readableBytes();  
            if(oldLen < highWaterMark_ && oldLen + remaining >= highWaterMark_ &&  highWaterMarkCallback_){
                // 如果以前积攒的数据不足水位 && 以前积攒的加上本次需要写入outputBuffer_的数据大于水位 && 注册了highWaterMarkCallback_
                // 调用highWaterMarkCallback_
                loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining)
                );
            }
            // 剩余没发送完的数据写入outputBuffer_
            outputBuffer_.append(static_cast<const char*>(data) + nwriten, remaining);
            if(!channel_->isWritingEvent()){
                // 给Channel注册EPOLLOUT写事件，否则当内核的TCP发送缓冲区没有数据时，Poller不会给Channel通知，Channel就不会调用writeCallback_，即TcpConnection::handleWrite
                channel_->enableWriting();
            }
        }
    }
}

// 客户端断开，或者有其他特殊情况，关闭当前TcpConnection
// shutdown给用户调用
void TcpConnection::shutdown(){
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, shared_from_this())
        );
    }
}

void TcpConnection::shutdownInLoop(){
    // 有可能数据还没发送完就调用了shutdown，先等待数据发送完，发送完后handleWrite内会调用shutdownInLoop
    if(!channel_->isWritingEvent()){
        // channel_对写事件不感兴趣，说明当前outputBuffer_中没有待发送的数据
        // 关闭写端
        socket_->shutdownWrite();
    }
}

// 建立连接
void TcpConnection::connectEstablished(){
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();               // 向Poller注册读事件

    connectionCallback_(shared_from_this()); // 用户传入的建立连接的回调on_connection
}

// 连接销毁
void TcpConnection::connectDestroyed(){
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll();   // 通过epoll_ctl删除fd
        connectionCallback_(shared_from_this());
    }
    channel_->remove();  // 从Poller中删除channel
}

// 有读事件到来，将数据写入inputBuffer_
void TcpConnection::handleRead(Timestamp receiveTime){
    int saveErrno = 0;
    // 发生了读事件，从channel_的fd中读取数据，存到inputBuffer_
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if(n > 0){
        // 读取完成后，需要调用用户传入的回调操作，就是我们给TcpServer传入的on_message函数
        // 把当前TcpConnection对象给用户定义的on_message函数，是因为用户需要利用TcpConnection对象给客户端发数据，inputBuffer_就是客户端发来的数据，也传入on_message函数给用户
        // shared_from_this就是获取了当前TcpConnection对象的一个shared_ptr
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }else if(n == 0){
        // readv返回0，说明客户端关闭了
        handleClose();
    }else{
        // n < 0
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead\n");
        handleError();
    }
}

// TcpConnection::sendInLoop一次write没有发送完数据，将剩余的数据写入outputBuffer_后，然后Channel调用writeCallback_
// Channel调用的writeCallback_就是TcpConnection注册的handleWrite，handleWrite用于继续发送outputBuffer_中的数据到TCP缓冲区，直到outputBuffer_可读区间没有数据
void TcpConnection::handleWrite(){
    if(channel_->isWritingEvent()){
        int saveErrno = 0;
        // 往fd上写outputBuffer_可读区间的数据，写了n个字节，即发送数据
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if(n > 0){
            outputBuffer_.retrieve(n);  // readerIndex_复位
            if(outputBuffer_.readableBytes() == 0){
                // outputBuffer_的可读区间为0，已经发送完了，将Channel封装的events置为不可写，底层还是调用的epoll_ctl
                // // Channel调用update remove ==> EventLoop updateChannel removeChannel ==> Poller updateChannel removeChannel
                channel_->disableWriting();
                if(writeCompleteCallback_){
                    // 唤醒loop_所在线程，执行数据发送完成以后的回调函数
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if(state_ == kDisconnecting){
                    // 读完数据时，如果发现已经调用了shutdown方法，state_会被置为kDisconnecting，则会调用shutdownInLoop，在当前所属的loop里面删除当前TcpConnection对象
                    shutdownInLoop();
                }
            }
        }else{
            // n <= 0
            LOG_ERROR("TcpConnection::handleWrite \n");
        }
    }else{
        // 要执行handleWrite，但是channel的fd的属性为不可写
        LOG_ERROR("TcpConnection::handleWrite fd=%d couldn`t writed \n", channel_->fd());
    }
}

void TcpConnection::handleClose(){
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();  // 对任何事件都不感兴趣，从epoll红黑树中删除

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);                  // 执行关闭连接（用户传入的）
    closeCallback_(connPtr);                       // 执行连接关闭以后的回调，即TcpServer::removeConnection
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof((optval)));
    int err = 0;

    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name : %s - SO_ERROR: %d \n", name_.c_str(), err);
}
