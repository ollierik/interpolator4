cmake_minimum_required(VERSION 3.16)
project(interpolator_test)

set(CMAKE_C_STANDARD 90) # ANSI C
set(CMAKE_C_STANDARD_REQUIRED True)

# static lib
set(SOURCES
    "src/interpolator4.h"
    "src/test.c"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Create a tree structure of all files in src dir
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC src)