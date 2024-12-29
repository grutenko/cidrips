cmake_minimum_required(VERSION 3.28)

project(cidrips VERSION 0.2.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libcidrips)
add_subdirectory(cidrips)
add_subdirectory(test)
