#include "noncopyable.h"

struct tcp_info;

class InetAddress;

class Socket : noncopyable{
    public:
        explicit Socket(int sockfd)
            : sockfd_(sockfd)
        {}
        ~Socket();

        int fd() const { return sockfd_; }
        void bindAddress(const InetAddress& localaddr);   // 将ip port绑定到listenfd
        void listen();
        int accept(InetAddress* peeraddr);                // 调用accept获取和客户通信的fd
        
        void shutdownWrite();                             // tcp socket是全双工通信，这是关闭监听套接字sockfd_写端

        void setTcpNoDelay(bool on);                      // 数据不进行tcp缓冲
        void setReuseAddr(bool on);
        void setReusePort(bool on);
        void setKeepAlive(bool on);

    private:
        const int sockfd_;  // 这就是服务器用于监听客户端的listenfd
};
