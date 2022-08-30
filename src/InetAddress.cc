#include "InetAddress.h"
#include <strings.h>
#include <string.h>

// 参数的默认值不管是声明还是定义，只能出现一次
InetAddress::InetAddress(uint16_t port, std::string ip){
    bzero(&addr_, sizeof(addr_));  // 清零addr_地址开始的sizeof(addr_)个字节
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);  // host to net ，本地字节序的port转成网络字节序的port，网络字节序都是大端，双方通信的时候需要都转换成统一的网络字节序，这样发送的数据就可以互相识别了
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());  // inet_addr：字符串转成整型的网络字节序32位的ip地址

}

std::string InetAddress::toIP() const{
    // 从addr_返回ip地址
    char buff[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buff, sizeof(buff));  // 从addr_.sin_addr读取32位整型的ip表示，转成本地字节序，存到buff
    return buff;
}

std::string InetAddress::toIpPort() const{
    // ip : port
    char buff[64] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buff, sizeof(buff));
    size_t ip_end = strlen(buff);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buff + ip_end, ":%u", port);
    return buff;
}

uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port);
}

const sockaddr_in* InetAddress::getSockAddr() const{
    return &addr_;
}

// #include <iostream>

// int main(){
//     InetAddress addr(8888);
//     std::cout << addr.toIpPort() << std::endl;
//     std::cout << addr.toIP() << std::endl;
//     std::cout << addr.toPort() << std::endl;
//     return 0;
// }
