# mymuduo
C++11重构muduo网络库

构建项目：./autobuild.sh

构建项目后，会在lib下生成libmymuduo.so，然后将include下的头文件拷贝到/usr/local/include/mymuduo，将so库拷贝到/usr/local/lib下

编译命令：
cd testcode
g++ -o testserver testserver.cc -std=c++11 -lmymuduo -lpthread

测试：./testserver 127.0.0.1 8989