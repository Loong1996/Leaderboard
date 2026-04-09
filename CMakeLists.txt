cmake_minimum_required(VERSION 3.15)
project(RankList LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(/utf-8)

add_executable(ranklist_demo src/main.cpp)
target_include_directories(ranklist_demo PRIVATE include/)

enable_testing()
add_subdirectory(test)
