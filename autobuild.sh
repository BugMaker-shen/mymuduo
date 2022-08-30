#!/bin/bash

set -e

# 如果没有build目录，创建该目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. &&
    make

# 回到项目根目录
cd ..

if [ ! -d /usr/local/include/mymuduo ]; then 
    mkdir /usr/local/include/mymuduo
fi

# 把头文件拷贝到 /usr/local/include/mymuduo
for header in `ls include/*.h`
do
    cp $header /usr/local/include/mymuduo
done

# so库拷贝到 /usr/local/lib    PATH
cp `pwd`/lib/libmymuduo.so /usr/local/lib

# 如果已经将so库拷贝到了PATH，编译器依然找不到库，需要执行一下ldconfig
ldconfig