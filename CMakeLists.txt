cmake_minimum_required(VERSION 2.5)
project(mymuduo)

# mymuduo最终编译成so动态库，设置动态库的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息，添加编译选项，启动C++11语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 配置头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 定义参与编译的源代码文件， 表示当前目录所有的源文件，把当前目录源文件组合起来用SRC_LIST记录
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)

# 配置可执行文件的输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 编译生成mymuduo动态库，最终会生成libmymuduo.so
add_library(mymuduo SHARED ${SRC_LIST})
