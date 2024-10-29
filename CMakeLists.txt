cmake_minimum_required(VERSION 3.7)
project(QCliTest)
set(CMAKE_C_STANDARD 11)

add_executable(qcli
        main.c
        qcli.c
        ringbuf.c
)