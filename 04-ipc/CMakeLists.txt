cmake_minimum_required(VERSION 3.16)
project(ipc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Each IPC mechanism is its own standalone demo -- unlike previous
# modules, there's no single library API to wrap these behind, since
# the whole point is two real OS processes communicating.
add_executable(01_pipe src/01_pipe.cpp)
target_compile_options(01_pipe PRIVATE -Wall -Wextra -Wpedantic -g)
