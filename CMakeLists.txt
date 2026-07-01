cmake_minimum_required(VERSION 3.20)
project(zinc_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(zinc_sim_lib
    src/decode.cpp
    src/elf_loader.cpp
)

target_include_directories(zinc_sim_lib PUBLIC include)

target_compile_options(zinc_sim_lib PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(zinc-sim
    src/main.cpp
)

target_link_libraries(zinc-sim PRIVATE
    zinc_sim_lib
)

target_compile_options(zinc-sim PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)