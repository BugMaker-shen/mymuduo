#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"
#include "EventLoop.h"

#include <memory>
#include <atomic>
#include <string.h>

class Channel;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>{
    public:
        // sockfd由TcpServer传入
        TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr);
        ~TcpConnection();

        // 返回所处的EventLoop
        EventLoop* getLoop() const { return loop_; }
        const std::string& name() const { return name_; }
        const InetAddress& localAddress() const { return localAddr_; }
        const InetAddress& peerAddress() const { return peerAddr_; }

        // 返回是否连接成功
        bool connected() const {return state_ == kConnected; }
        bool disconnected() const {return state_ == kDisconnected; }

        void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
        void setWriteCompleteCallback_(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
        void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
        void setHighWaterMarkCallback_(const HighWaterMarkCallback& cb, size_t highWaterMark) { 
            highWaterMarkCallback_ = cb;
            highWaterMark_ = highWaterMark;
        }

        void send(const std::string& buff);
        void shutdown();

        // 建立连接
        void connectEstablished();
        // 连接销毁
        void connectDestroyed();

    private:
        // 枚举连接的状态
        // 初始化时kConnecting，连接成功kConnected，断开连接shutdown kDisconnecting，底层的fd关闭后kDisconnected
        enum StateE{kConnected, kDisconnected, kConnecting, kDisconnecting};  
        void setState(StateE state){
            state_ = state;
        }
        
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        // 发送数据
        void sendInLoop(const void* data, size_t len);
        // 客户端断开，或者有其他特殊情况，关闭当前TcpConnection
        void shutdownInLoop();

        EventLoop* loop_;                                  //这绝对不是baseLoop， 因为TcpConnection都是在subLoop里面管理的
        const std::string name_;
        std::atomic_int state_;                            // 会在多线程环境使用
        bool reading_;

        std::unique_ptr<Socket> socket_;                   // 这俩成员和Acceptor的成员类似，Acceptor处于mainLoop，TcpConnection处于subLoop
        std::unique_ptr<Channel> channel_;

        const InetAddress localAddr_;                      // 主机地址，这是定义变量，编译阶段需要知道变量占用空间，要包含头文件
        const InetAddress peerAddr_;                       // 客户端地址

        ConnectionCallback connectionCallback_;            // 有新连接和关闭连接的回调处理函数，就是用户传入的on_connection
        MessageCallback messageCallback_;                  // 已连接用户的读写消息回调处理函数，就是用户传入的on_message
        WriteCompleteCallback writeCompleteCallback_;      // 消息发送完成的回调处理函数
        CloseCallback closeCallback_;
        HighWaterMarkCallback highWaterMarkCallback_;      // 控制双方发送、接收速度
        size_t highWaterMark_;                             // 水位线

        Buffer inputBuffer_;                               // 用于服务器接收数据，handleRead就是写入inputBuffer_
        Buffer outputBuffer_;                              // 用于服务器发送数据，应用层需要发送的数据先存到outputBuffer_
};
