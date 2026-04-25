cmake_minimum_required(VERSION 3.16)

project(spectre VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(third_party/raylib-5.5)

set(KISSFFT_DATATYPE float CACHE STRING "" FORCE)
set(KISSFFT_STATIC   ON    CACHE BOOL "" FORCE)
set(KISSFFT_TOOLS    OFF   CACHE BOOL "" FORCE)
set(KISSFFT_TEST     OFF   CACHE BOOL "" FORCE)
add_subdirectory(third_party/kissfft-131.2.0)

set(CLF_QUEUE_SIZE 4096 CACHE STRING "in terms of floats" FORCE)
add_subdirectory(modules/LockFreeQueue)

SET(TESTING OFF CACHE BOOL "compile tests")
if (TESTING)
        enable_testing()
        add_subdirectory(test)
endif()
