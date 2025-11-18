cmake_minimum_required(VERSION 3.12) 
project(lab2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(${PROJECT_NAME} main.cpp some_class.cpp)