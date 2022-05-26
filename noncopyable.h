#pragma once

/**
 * 继承noncopyable的类对象，可以进行构造和析构，无法进行拷贝构造和赋值操作
 */
class noncopyable{
    public:
        noncopyable(const noncopyable&) = delete;
        noncopyable& operator=(const noncopyable&) = delete;
    protected:
        noncopyable() = default;
        ~noncopyable() = default;
};
