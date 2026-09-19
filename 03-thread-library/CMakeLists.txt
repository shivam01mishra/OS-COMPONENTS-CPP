cmake_minimum_required(VERSION 3.16)
project(thread_library CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(thread_library
    src/thread.cpp
    src/scheduler.cpp
    src/main.cpp
)

target_include_directories(thread_library PRIVATE include)
target_compile_options(thread_library PRIVATE -Wall -Wextra -Wpedantic -g)
