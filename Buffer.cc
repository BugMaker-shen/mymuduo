#include "Buffer.h"

#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>

ssize_t Buffer::readFd(int fd, int* saveErrno){
    char extrabuff[65536] = {0};  // 64K栈空间，会随着函数栈帧回退，内存自动回收
    struct iovec vec[2];

    const size_t writable = writableBytes();   // Buffer底层剩余的可写空间

    vec[0].iov_base = begin() + writerIndex_;  // 第一块缓冲区
    vec[0].iov_len = writable;                 // iov_base缓冲区可写的大小

    vec[1].iov_base = extrabuff;               // 第二块缓冲区
    vec[1].iov_len = sizeof(extrabuff);
	
	// 如果Buffer有65536字节的空闲空间，就不使用栈上的缓冲区，如果不够65536字节，就使用栈上的缓冲区，即readv一次最多读取65536字节数据
    const int iovcnt = (writable < sizeof(extrabuff)) ? 2 : 1;  
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        *saveErrno = errno;
    }else if(n <= writable){
        // 读取的数据n小于Buffer底层的可写空间，readv会直接把数据存放在begin() + writerIndex_
        writerIndex_ += n;
    }else{
        // Buffer底层的可写空间不够存放n字节数据，extrabuff有部分数据（n - writable）
        // 从缓冲区末尾再开始写
        writerIndex_ = buffer_.size();
        // 从extrabuff里读取 n - writable 字节的数据存入Buffer底层的缓冲区
        append(extrabuff, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno){
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0){
        *saveErrno = errno;
    }
    return n;
}