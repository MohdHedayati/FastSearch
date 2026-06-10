cmake_minimum_required(VERSION 3.15)
project(FastSearch VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror -O3 -march=native)
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
include_directories(include)

# 1. Build the Headless Server (Daemon)
add_executable(fast_searchd
    src/main.cpp
    src/os/crawler.cpp
    src/os/watcher.cpp
    src/os/ipc_server.cpp
    src/core/trie.cpp
    src/core/inverted_index.cpp
)
target_link_libraries(fast_searchd PRIVATE stdc++fs)

# 2. Build the Lightweight CLI Client
add_executable(fs 
    src/client.cpp
)