#pragma once

#include <vector>
#include <string>
#include <algorithm>

class Buffer{
    public:
        static const size_t kCheapPrepend = 8;   // 数据包长度用8字节存储
        static const size_t kInitailSize = 1024; // 缓冲区初始大小

        explicit Buffer(size_t initialSize = kInitailSize)
            : buffer_(kCheapPrepend + initialSize)              // 底层vector的长度
            , readerIndex_(kCheapPrepend)
            , writerIndex_(kCheapPrepend)
        {}

        // 待读取数据长度
        const size_t readableBytes() const{
            return writerIndex_ - readerIndex_;                 
        }

        // 可写空闲大小
        const size_t writableBytes() const{
            return buffer_.size() - writerIndex_;               
        }

        const size_t prependableBytes() const{
            return readerIndex_;
        }

        // 返回缓冲区中可读数据的起始地址
        const char* peek() const{
            return begin() + readerIndex_;
        }

        void retrieve(size_t len){
            // len就是应用程序从Buffer缓冲区读取的数据长度
            // 必须要保证len <= readableBytes()
            if(len < readableBytes()){
                // 这里就是可读数据没有读完
                readerIndex_ += len;
            }else{
                // len == readableBytes()
                // 可读数据读完了，readerIndex_和writerIndex_都要复位
                retrieveAll();
            }
        }

        void retrieveAll(){
            readerIndex_ = kCheapPrepend;
            writerIndex_ = kCheapPrepend;
        }

        std::string retrieveAsString(size_t len){
            if(len <= readableBytes()){
                std::string result(peek(), len);   // peek返回缓冲区中可读数据的起始地址，从可读地址开始截取len个字符
                retrieve(len);                     // result保存了缓冲区中的可读数据，retrieve将参数复位
                return result;
            }
        }

        // onMessage 有数据到来时，把数据打包成Buffer对象，retrieveAllAsString把Buffer类型转成string
        std::string retrieveAllAsString(){
            return retrieveAsString(readableBytes());
        }

        // 可写缓冲区为[writerIndex, buffer_.size()]，需要写入的数据长度为len
        void ensureWritableBytes(size_t len){
            if(writableBytes() < len){
                makeSpace(len);     // 扩容
            }
        }

        void append(const char* data, size_t len){
            // 确保可写空间不小于len
            ensureWritableBytes(len);
            // 把data中的数据往可写的位置writerIndex_写入
            std::copy(data, data+len, beginWrite());
            // len字节的data写入到Buffer后，就移动writerIndex_
            writerIndex_ += len;
        }
        
        char* beginWrite() {
            return begin() + writerIndex_;
        }

        const char* beginWrite() const{
            return begin() + writerIndex_;
        }

        // 从fd上读取数据，存放到writerIndex_，返回实际读取的数据大小
        ssize_t readFd(int fd, int* saveErrno);
        ssize_t writeFd(int fd, int* saveErrno);


    private:
        // 返回Buffer底层数据首地址
        char* begin(){
            return &(*buffer_.begin());
        }

        // 常对象只能调用常方法，不能调用普通方法
        const char* begin() const {
            return &(*buffer_.begin());
        }

        void makeSpace(size_t len){
            // writableBytes() + prependableBytes() < len + kCheapPrepend
            if(buffer_.size() - (writerIndex_ - readerIndex_) < len + kCheapPrepend){
                buffer_.resize(writerIndex_ + len);
            }else{
                size_t readable = readableBytes();
                std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
                readerIndex_ = kCheapPrepend;
                writerIndex_ = readerIndex_ + readable;
            }
        }

        std::vector<char> buffer_;                               // vector管理的资源自动释放，Buffer对象在哪个区，buffer_就在哪个区，主要利用vector自动扩容的功能
        size_t readerIndex_;
        size_t writerIndex_;
};
