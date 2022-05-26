#include <mymuduo/TcpServer.h>

#include <string>
#include <functional>

class EchoServer{
    public:
        EchoServer(EventLoop* loop, const InetAddress& addr, const std::string &name)
            : server_(loop, addr, name)
            , loop_(loop)
        {
            // 注册回调函数
            server_.setConnectionCallback(std::bind(&EchoServer::on_connection, this, std::placeholders::_1));
            server_.setMessageCallback(std::bind(&EchoServer::on_message, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            
            // 设置合适的subloop线程
            server_.setThreadNum(3);
        }

        void start(){
            server_.start();
        }

    private:
        void on_connection(const TcpConnectionPtr& conn){
            if(conn->connected()){
                LOG_INFO("conn up : %s \n", conn->peerAddress().toIpPort().c_str());
            }else{
                LOG_INFO("conn down : %s \n", conn->peerAddress().toIpPort().c_str());
            }
        }

        void on_message(const TcpConnectionPtr& conn, Buffer* buff, Timestamp time){
            std::string msg = buff->retrieveAllAsString();
            conn->send(msg);
            conn->shutdown();  // 关闭写端，触发EPOLLHUP，Channel调用closeCallback_
        }

        EventLoop* loop_;
        TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(8888, "127.0.0.1");
    EchoServer server(&loop, addr, "EchoServer-01");  // Acceptor non-blocking listenfd
    server.start();   // 会将listenfd封装成acceptChannel，添加到mainloop
    loop.loop();     // 启动EpollPoller

    return 0;
}