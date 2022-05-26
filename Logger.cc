#include <iostream>

#include "Logger.h"
#include "Timestamp.h"

// 获取唯一的日志实例对象
Logger& Logger::instance(){
    static Logger logger; // static保存在数据段，只会生成一个实例
    return logger;
}

// 设置日志级别
void Logger::set_log_level(int level){
    log_level_ = level;
}

// 写日志 [级别] time : msg
void Logger::log(std::string msg){
    switch (log_level_){
        case INFO:
            std::cout<<"[INFO]";
            break;
        case ERROR:
            std::cout<<"[ERROR]";
            break;
        case FATAL:
            std::cout<<"[FATAL]";
            break;
        case DEBUG:
            std::cout<<"[DEBUG]";
            break;
        default:
            break;
    }

    // 打印时间和msg
    std::cout<< Timestamp::now().toString() << " : " << msg << std::endl;
}

