#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread{
    extern __thread int t_cachedTid;

    void cacheTid();

    // 内联只在当前文件起作用
    inline int tid(){
        if(__builtin_expect(t_cachedTid == 0, 0)){
            // 如果t_cachedTid == 0，表示还没有获取tid，需要通过系统调用syscall获取
            cacheTid();
        }
        return t_cachedTid;
    }
}
