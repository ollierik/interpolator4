cmake_minimum_required(VERSION 3.16)
project(interpolator_test)

set(CMAKE_C_STANDARD 90) # ANSI C
set(CMAKE_C_STANDARD_REQUIRED True)

# static lib
set(SOURCES
    "include/interp4/interp4ff.h"
    #"include/interp4/interp4fd.h"
    #"include/interp4/interp4df.h"
    #"include/interp4/interp4dd.h"
    "test/test.c"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Create a tree structure of all files in src dir
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC "include/interp4")