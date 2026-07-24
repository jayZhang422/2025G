cmake_minimum_required(VERSION 3.20)
project(Identification_Processing C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# 1. 抓取源码
file(GLOB_RECURSE SOURCES
        "Identification_Processing_System/src/*.c"
        "Identification_Processing_System/src/*.h"
        "Identification_Processing_System/src/*.cpp"
)

add_executable(Identification_Processing ${SOURCES})

# ================= 核心魔法区：自动提取包含路径 =================

# 自动扫描工作区内所有的 .h 文件（包括你的 src 和整个 Platform 文件夹）
file(GLOB_RECURSE ALL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(AUTO_INCLUDE_DIRS "")
foreach(_headerFile ${ALL_HEADERS})
    # 获取每个 .h 文件所在的目录路径
    get_filename_component(_dir ${_headerFile} PATH)
    # 加入到列表中
    list(APPEND AUTO_INCLUDE_DIRS ${_dir})
endforeach()

# 去除重复的路径，防止列表爆炸
if(AUTO_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES AUTO_INCLUDE_DIRS)
endif()

# ==============================================================

# 2. 包含路径：自动路径 + 编译器底层库
target_include_directories(Identification_Processing PRIVATE
        ${AUTO_INCLUDE_DIRS} # 这里直接把上面自动扫描到的所有路径塞进去！

        # 仅需保留 Vitis 安装目录下的底层标准库 (解决 math.h 等找不到的问题)
        "D:/FPGA/Xilinx/Vitis/2020.2/gnu/aarch32/nt/gcc-arm-none-eabi/aarch32-xilinx-eabi/usr/include"
        "D:/FPGA/Xilinx/Vitis/2020.2/gnu/aarch32/nt/gcc-arm-none-eabi/x86_64-oesdk-mingw32/usr/lib/arm-xilinx-eabi/gcc/arm-xilinx-eabi/9.2.0/include"
)

target_compile_definitions(Identification_Processing PRIVATE _DEBUG UNICODE _UNICODE)