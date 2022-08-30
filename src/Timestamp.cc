#include "Timestamp.h"

Timestamp::Timestamp()
    :microSecondsSinceEpoch_(0)
{}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
{}

Timestamp Timestamp::now(){
    // 用time_t类型构造Timestamp类型，并返回Timestamp对象
    return Timestamp(time(NULL));
}

std::string Timestamp::toString() const{
    char buff[128] = {0};
    tm* tm_time = localtime(&microSecondsSinceEpoch_); // 时间戳转换为结构体tm类型
    snprintf(buff, 128, "%4d-%02d-%02d %02d:%02d:%02d", 
        tm_time->tm_year + 1900,
        tm_time->tm_mon + 1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec);
    return buff;
}

// #include <iostream>

// int main(){
//     Timestamp ts(time(NULL));
//     std::cout<<ts.toString()<<std::endl;
//     return 0;
// }
