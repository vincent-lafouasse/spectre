cmake_minimum_required(VERSION 3.16)

project(spectre VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
        src/main.c
)

target_include_directories(${PROJECT_NAME} PRIVATE src src/common)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -g3
                                               -fdiagnostics-color=always)


SET(TESTING OFF CACHE BOOL "compile tests")
if (TESTING)
        enable_testing()
        add_subdirectory(test)
endif()
