#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable{
    public:
        using NewConnetionCallback = std::function<void(int sockfd, const InetAddress&)>;
        
        Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
        ~Acceptor();

        void setNewConnectionCallback(const NewConnetionCallback& cb){
            newConnetionCallback_ = std::move(cb);
        }

        bool listening(){ return listening_; }
        void listen();

    private:
        void handleRead();

        EventLoop* loop_;
        Socket acceptSocket_;
        Channel acceptChannel_;
        NewConnetionCallback newConnetionCallback_;
        bool listening_;
};
