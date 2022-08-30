#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

class InetAddress{
    public:
        explicit InetAddress(uint16_t port = 8888, std::string ip = "127.0.0.1");
        explicit InetAddress(const sockaddr_in& addr)
            : addr_(addr)
        {}

        std::string toIP() const;
        std::string toIpPort() const;
        uint16_t toPort() const;

        const sockaddr_in* getSockAddr() const;
		void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }
		
    private:
        sockaddr_in addr_;  // 存储的都是网络字节序的协议信息
};
