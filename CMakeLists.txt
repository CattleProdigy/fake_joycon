cmake_minimum_required(VERSION 3.1)
project(fake_joycon)

find_package(Threads)
set(CMAKE_C_STANDARD 11)
set(C_EXTENSIONS ON)

add_executable(device device.c)
target_link_libraries(device PRIVATE Threads::Threads)
target_compile_options(device PRIVATE -Wall -Wextra)
