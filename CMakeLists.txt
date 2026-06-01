cmake_minimum_required(VERSION 3.20)

project(zinc-simulator LANGUAGES CXX)

add_executable(zinc-simulator
    src/main.cpp
    src/binary_loader.cpp
    src/memory.cpp
    src/rv64_core.cpp
)

target_compile_features(zinc-simulator PRIVATE cxx_std_20)

target_include_directories(zinc-simulator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(zinc-simulator PRIVATE /W4)
else()
    target_compile_options(zinc-simulator PRIVATE -Wall -Wextra -Wpedantic)
endif()
