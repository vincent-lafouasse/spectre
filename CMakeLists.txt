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

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
        src/main.c

        src/FFTAnalyzer.c
        src/RMSVisualizer.c
        src/LinearSpectrogram.c
        src/RMSAnalyzer.c
        src/audio_callback.c

        src/core/History.c
        src/core/colormap/colormap.c

        src/dsp/window.h
        src/dsp/filters.c
        src/dsp/filters.h
        src/dsp/window.c
)

target_include_directories(${PROJECT_NAME} PUBLIC src)

target_link_libraries(${PROJECT_NAME} PRIVATE LockFreeQueue)
target_link_libraries(${PROJECT_NAME} PRIVATE kissfft)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if (APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit")
        target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Cocoa")
        target_link_libraries(${PROJECT_NAME} PRIVATE "-framework OpenGL")
endif()

SET(TESTING OFF CACHE BOOL "compile tests")
if (TESTING)
        enable_testing()
        add_subdirectory(test)
endif()
