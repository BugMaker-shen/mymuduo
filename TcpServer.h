#pragma once

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <strings.h>
#include <sys/socket.h>

class TcpServer : noncopyable{
    public:
        using ThreadInitCallback = std::function<void(EventLoop*)>;

        // 预置两个是否重用端口的选项
        enum Option{
            kNoReusePort,
            kReusePort,
        };

        // 构造函数，用户需要传入一个EventLoop，即mainLoop，还有服务器的ip和监听的端口
        TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option = kNoReusePort);
        ~TcpServer();

        // 线程初始化回调
        void setThreadInitCallback(const ThreadInitCallback& cb){ threadInitCallback_ = cb; }
        void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
        void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
        void setWriteCompleteCallback(const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }

        // 设置subloop的数量
        void setThreadNum(int numThreads);
        // 开启服务器监听
        void start();

    private:
        void newConnection(int connfd, const InetAddress& peerAddr);         // 一条新连接到来
        void removeConnection(const TcpConnectionPtr& conn);                 // 连接已断开，从ConnectionMap里面移除
        void removeConnectionInLoop(const TcpConnectionPtr& conn);           // 从事件循环中删除连接

        using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

        const std::string ipPort_;                         // 保存服务器的ip port
        const std::string name_;                           // 保存服务器的name
        EventLoop* loop_;                                  // 用户传入的baseloop
        std::unique_ptr<Acceptor> acceptor_;               // 运行在mainloop的Acceptor，用于监听listenfd，等待新用户连接
        std::shared_ptr<EventLoopThreadPool> threadPool_;  // one loop per thread

        ThreadInitCallback threadInitCallback_;            // loop线程初始化的回调
        ConnectionCallback connectionCallback_;            // 有新连接的回调处理函数
        MessageCallback messageCallback_;                  // 已连接用户的读写消息回调处理函数
        WriteCompleteCallback writeCompleteCallback_;      // 消息发送完成的回调处理函数

        std::atomic_int started_;
        int nextConnId_;
        ConnectionMap connections_;                        // 保存所有的连接 
};