cmake_minimum_required(VERSION 3.22)
project(lumen VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_Debug   "-g3 -fsanitize=address,undefined -fno-omit-frame-pointer")

add_executable(lumen
    src/lexer.cpp
    src/parser.cpp
    src/compiler.cpp
    src/vm.cpp
    src/main.cpp
)

target_include_directories(lumen PRIVATE include)
target_compile_options(lumen PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)

message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
