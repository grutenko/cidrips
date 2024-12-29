cmake_minimum_required(VERSION 3.28)

add_library(libcidrips OBJECT src/addr.c src/alloc.c src/bc134.c src/list.c src/tree.c src/compress.c)
target_include_directories(libcidrips PUBLIC include)
