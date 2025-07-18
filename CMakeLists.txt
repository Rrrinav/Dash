cmake_minimum_required(VERSION 3.16)
project(data_tree_project CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Source files
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SOURCES
    ${SRC_DIR}/data_tree.cpp
    ${SRC_DIR}/tree_format.cpp
    ${CMAKE_SOURCE_DIR}/main.cpp
)

add_executable(data_tree_exec ${SOURCES})
