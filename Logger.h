#pragma once

#include <string>

#include "noncopyable.h"

// 正式项目中写宏的时候，为了防止产生错误，都会写成do...while(0)，有多行的话，行末需要加上\，并且\后不能有空格
// LogMsgFormat时格式化字符串，##__VA_ARGS__用于获取可变参列表
#define LOG_INFO(LogMsgFormat, ...) \
    do{\
        Logger &logger = Logger::instance();\
        logger.set_log_level(INFO);\
        char buff[1024] = {0};\
        snprintf(buff, 1024, LogMsgFormat, ##__VA_ARGS__);\
        logger.log(buff);\
    }while(0)

#define LOG_ERROR(LogMsgFormat, ...) \
    do{\
        Logger &logger = Logger::instance();\
        logger.set_log_level(ERROR);\
        char buff[1024] = {0};\
        snprintf(buff, 1024, LogMsgFormat, ##__VA_ARGS__);\
        logger.log(buff);\
    }while(0)

#define LOG_FATAL(LogMsgFormat, ...) \
    do{\
        Logger &logger = Logger::instance();\
        logger.set_log_level(FATAL);\
        char buff[1024] = {0};\
        snprintf(buff, 1024, LogMsgFormat, ##__VA_ARGS__);\
        logger.log(buff);\
    }while(0)

// 由于调试信息通常较多，我们给出开关宏MUDUO_DEBUG
#ifdef MUDUO_DEBUG
#define LOG_DEBUG(LogMsgFormat, ...) \
    do{\
        Logger &logger = Logger::instance();\
        logger.set_log_level(DEBUG);\
        char buff[1024] = {0};\
        snprintf(buff, 1024, LogMsgFormat, ##__VA_ARGS__);\
        logger.log(buff);\
    }while(0)
#else
    #define LOG_DEBUG(LogMsgFormat, ...)
#endif

enum LogLevel{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

// 日志类，写成单例，不需要进行拷贝构造和赋值
class Logger : noncopyable{
    public:
        // 获取唯一的日志实例对象
        static Logger& instance();
        // 设置日志级别
        void set_log_level(int level);
        // 写日志
        void log(std::string msg);
    private:
        int log_level_;
        // 单例模式需要将构造函数私有化
        Logger(){}
};

