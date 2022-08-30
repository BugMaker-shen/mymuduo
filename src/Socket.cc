#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

Socket::~Socket(){
    ::close(sockfd_);
}

// 将服务器本地ip port绑定到listenfd
void Socket::bindAddress(const InetAddress& localaddr){
    if(0 > ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in))){
        LOG_FATAL("bind sockfd:%d fail \n", sockfd_);
    }
}

void Socket::listen(){
    if(0 > ::listen(sockfd_, 1024)){
        LOG_FATAL("listen sockfd:%d fail \n", sockfd_);
    }
}

// 调用accept获取和客户通信的fd
// 上层调用accept，传入InetAddress对象peeraddr的地址，并且把客户的addr写入输出参数peeraddr
int Socket::accept(InetAddress* peeraddr){
    // 写accept时，犯的错误有：len没初始化，返回的connfd没有设置非阻塞
    sockaddr_in addr;
    socklen_t len = sizeof(addr);

    ::memset(&addr, 0, sizeof(addr));
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0){
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

// tcp socket是全双工通信，这是关闭监听套接字sockfd_写端
void Socket::shutdownWrite(){
    // 关闭监听套接字的写端
    if(::shutdown(sockfd_, SHUT_WR)){
        LOG_ERROR("Socket::shutdownWrite error");
    }
}

// 数据不进行tcp缓冲
void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1 : 0;
    // TCP_NODELAY禁用Nagle算法，TCP_NODELAY包含在 <netinet/tcp.h>
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}
