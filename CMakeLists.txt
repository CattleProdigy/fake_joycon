cmake_minimum_required(VERSION 3.1)
project(fake_joycon)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(C_EXTENSIONS ON)

find_package(PkgConfig)
find_package(Threads)

pkg_check_modules(CZMQ REQUIRED libczmq)

add_executable(device device.c)
target_link_libraries(device PRIVATE Threads::Threads)
target_compile_options(device PRIVATE -g -o -Wall -Wextra)

add_executable(client beacon_client.cpp)
target_link_libraries(client PRIVATE ${CZMQ_LIBRARIES} Threads::Threads)
target_include_directories(client PRIVATE ${CZMQ_INCLUDE_DIRS})
target_compile_options(client PRIVATE -g -o -Wall -Wextra)

add_executable(serv beacon_server.cpp)
target_link_libraries(serv PRIVATE ${CZMQ_LIBRARIES})
target_include_directories(serv PRIVATE ${CZMQ_INCLUDE_DIRS})
target_compile_options(serv PRIVATE -g -o -Wall -Wextra)
