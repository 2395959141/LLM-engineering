cmake_minimum_required(VERSION 3.26 FATAL_ERROR)

# CUDA 路径设置
set(CMAKE_CUDA_COMPILER /usr/local/cuda-12.6/bin/nvcc)
set(CUDA_TOOLKIT_ROOT_DIR /usr/local/cuda-12.6)
set(CUDA_BIN_PATH /usr/local/cuda-12.6/bin)

# 项目定义
project(oneLLM LANGUAGES CXX CUDA)

# 查找 CUDA 包
find_package(CUDA 12.6 REQUIRED)
set(CUDA_PATH ${CUDA_TOOLKIT_ROOT_DIR})
list(APPEND CMAKE_MODULE_PATH ${CUDA_PATH}/lib64)
find_package(CUDA REQUIRED)

# 编译器标志设置
set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS}")	
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}  -Xcompiler -Wall")

set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS}  \
                      -gencode=arch=compute_89,code=\\\"sm_89,compute_89\\\" \
                        ")
#                      -rdc=true") # not sure the effect of this option, retain it temply

set(CMAKE_CUDA_ARCHITECTURES 89)
message("-- Assign GPU architecture (sm=89)")

# 调试和发布模式标志
set(CMAKE_C_FLAGS_DEBUG    "${CMAKE_C_FLAGS_DEBUG}    -Wall -O0")
set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG}  -Wall -O0")
set(CMAKE_CUDA_FLAGS_DEBUG "${CMAKE_CUDA_FLAGS_DEBUG} -O0 -G -Xcompiler -Wall") # -Wall：启用所有常见的编译器警告
                                                                        # 告诉 CUDA 编译器将后面的选项传递给主机编译器（通常是 gcc 或 clang）

message(STATUS "CMAKE_CXX_FLAGS" ${CMAKE_CXX_FLAGS})

# C++ 标准设置
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_STANDARD STREQUAL "11")
#为 CUDA 编译器添加相应的标志以支持 C++11 特性
  set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-extended-lambda")
  set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-relaxed-constexpr")
  set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --std=c++11")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CUDA_FLAGS_RELEASE "${CMAKE_CUDA_FLAGS_RELEASE} -Xcompiler -O3")

#输出目录设置
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 头文件目录设置
set(COMMON_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}
  ${CUDA_PATH}/include
)
# 库文件目录设置
set(COMMON_LIB_DIRS
  ${CUDA_PATH}/lib64
)

# 包含头文件目录
include_directories(
  ${COMMON_HEADER_DIRS}
)
# 链接库文件目录
link_directories(
  ${COMMON_LIB_DIRS}
)
# 性能选项
option (PERF
  "measure model inference performance"
  OFF
)
# 打印数据选项
option (PRINT_DATA
  "print kernel output to debug"
  OFF
)
# 保存数据选项
option (SAVE_DATA
  "save kernel output to debug"
  OFF
)
if (PERF)
    add_compile_options(-DPERF)
endif()
if (PRINT_DATA)
    add_compile_options(-DPRINT_DATA)
endif()
if (SAVE_DATA)
    add_compile_options(-DSAVE_DATA)
endif()
#cmake .. -DPRINT_DATA=ON && make
#cmake .. -DPRINT_DATA=ON -DSAVE_DATA=ON && make
#cmake .. -DPERF=ON && make
#cmake .. && make

# 源文件收集
file(GLOB_RECURSE LLM_CXX_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB_RECURSE LLM_CUDA_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cu)

# 库和可执行文件构建
add_library(llmengine OBJECT
           ${LLM_CXX_SOURCES}
           ${LLM_CUDA_SOURCES}
           )

add_subdirectory(src)
add_subdirectory(tests)
# add_subdirectory(examples)

# 可执行文件构建
#创建一个名为 llmengine 的对象库，包含所有 C++ 和 CUDA 源文件。
#然后添加 src 和 tests 子目录（examples 被注释掉了）。
#最后，创建一个名为 main 的可执行文件，链接 CUDA 库和 llmengine 库。
add_executable(main user_entry.cpp)
target_link_libraries(main PUBLIC -lcublas -lcudart -lcudadevrt llmengine)
