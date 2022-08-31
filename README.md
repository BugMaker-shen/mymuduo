# mymuduo
C++11重构muduo网络库

构建项目：./autobuild.sh<br>

执行构建项目的脚本后，会在lib下生成libmymuduo.so，然后将include下的头文件拷贝到/usr/local/include/mymuduo，将so库拷贝到/usr/local/lib下（脚本完成）<br>

编译命令：
cd testcode<br>
g++ -o testserver testserver.cc -std=c++11 -lmymuduo -lpthread<br>

测试：./testserver 127.0.0.1 8989
