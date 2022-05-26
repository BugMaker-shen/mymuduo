#include "TcpServer.h"
#include "TcpConnection.h"

static EventLoop* CheckLoopNotNull(EventLoop* loop){
    if(loop == nullptr){
        LOG_FATAL("%s:%s%d mainloop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

// 构造函数，用户需要传入一个EventLoop，即mainLoop，还有服务器的ip和监听的端口
TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))                         // EventLoopThreadPool线程池
    , connectionCallback_()                                                     // 
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{   
    // 有新用户连接时，会调用Acceptor::handleRead，然后handleRead调用TcpServer::newConnection，
    // 使用两个占位符，因为TcpServer::newConnection方法需要新用户的connfd以及新用户的ip port
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}


TcpServer::~TcpServer(){
    for (auto& item : connections_){
        // 这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源了
        TcpConnectionPtr tmp_conn(item.second);
        // TcpServer::connections_的值是shared_ptr，让TcpServer::connections_的值，即item.second不再指向TcpConnection对象，然后就只剩局部TcpConnectionPtr指向TcpConnection对象
        // __shared_ptr().swap(*this)
        item.second.reset();

        // 销毁连接
        tmp_conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, tmp_conn));
    }
}

 
//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

//开启服务器监听     loop.loop()
void TcpServer::start(){
    // 防止一个TcpServer对象被start多次，只有第一次调用start才能进入if
    if (started_++ == 0){
        threadPool_->start(threadInitCallback_);//启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));  // 这就是TCP编程直接调用listen了
    }
}


// 一条新连接到来，根据轮询算法选择一个subloop并唤醒，把和客户端通信的connfd封装成Channel分发给subloop
// TcpServer要把newConnection设置给Acceptor，让Acceptor对象去调用，工作在mainLoop
// connfd是用于和客户端通信的fd，peerAddr封装了客户端的ip port
void TcpServer::newConnection(int connfd, const InetAddress& peerAddr){
    // 轮询算法获取一个subloop指针
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buff[64];
    snprintf(buff, sizeof(buff), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    // newConnection的名称
    std::string connName = name_ + buff;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n", name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机ip和port
    sockaddr_in local;
    memset(&local, 0, sizeof(local));
    socklen_t addr_len = sizeof(local);
    // getsockname通过connfd拿到本地的sockaddr地址信息写入local
    if(::getsockname(connfd, (sockaddr*)&local, &addr_len) < 0){
        LOG_ERROR("sockets::getLocalAddr \n");
    }
    InetAddress localAddr(local);

    // 将connnfd封装成TcpConnection，TcpConnection有一个Channel的成员变量，这里就相当于把一个TcpConnection对象放入了一个subloop
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, connfd, localAddr, peerAddr));
    // 将新的TcpConnectionPtr存入TcpServer::connections_
    connections_[connName] = conn;

    // 下面的回调都是用户设置给TcpServer的，然后TcpServer => TcpConnection => Channel，Channel会把自己封装的fd和events注册到Poller，发生事件时Poller调用Channel的handleEvent方法处理
    // 就比如这个messageCallback_，用户把on_message（messageCallback_）传给TcpServer，TcpServer会调用TcpConnection::setMessageCallback，那么TcpConnection的成员messageCallback_就保存了on_message
    // TcpConnection会把handleRead设置到Channel的readCallBack_，而handleRead就包括了TcpConnection::messageCallback_（on_message）
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback_(writeCompleteCallback_);
    // 设置了如何关闭连接的回调   conn->shutDown()
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 只要有一个客户端连接，就会直接调用TcpConnection::connectEstablished，connectEstablished就是把Channel对应的fd注册到Poller
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

// 连接已断开，从ConnectionMap里面移除
void TcpServer::removeConnection(const TcpConnectionPtr& conn){
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// 从事件循环中删除连接
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop(); 
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
