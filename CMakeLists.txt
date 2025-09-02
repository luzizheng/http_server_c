cmake_minimum_required(VERSION 3.10)
project(MultiProtocolServer VERSION 1.0)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 添加编译选项，开启警告并进行优化
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -g -O0")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -O2")
endif()

# 定义源文件
set(SOURCES
    server.c
    http_server.c
    ftp_server.c
)

# 定义头文件
set(HEADERS
    server.h
    http_server.h
    ftp_server.h
    config.h
)

# 添加可执行目标
add_executable(server ${SOURCES} ${HEADERS})

# 链接线程库
target_link_libraries(server pthread)

# 安装配置（可选）
install(TARGETS server
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# 若需要创建编译目录
# 在项目根目录执行: mkdir build && cd build && cmake .. && make
