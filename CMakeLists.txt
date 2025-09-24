cmake_minimum_required(VERSION 3.7)
project(qcli)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

file(GLOB SRC
    ${CMAKE_SOURCE_DIR}/*.cpp
    ${CMAKE_SOURCE_DIR}/*.h
    ${CMAKE_SOURCE_DIR}/*.hpp
    ${CMAKE_SOURCE_DIR}/*.c
)

add_executable(${PROJECT_NAME}
    ${SRC}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
)
