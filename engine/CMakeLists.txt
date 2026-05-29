cmake_minimum_required(VERSION 3.20)
project(pawnGO VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -flto")

include_directories(
    ${CMAKE_SOURCE_DIR}/src 
    ${CMAKE_SOURCE_DIR}/src/core 
    ${CMAKE_SOURCE_DIR}/src/search 
    ${CMAKE_SOURCE_DIR}/src/eval 
    ${CMAKE_SOURCE_DIR}/src/utils 
    ${CMAKE_SOURCE_DIR}/src/eval/nnue/src 
    ${CMAKE_SOURCE_DIR}/src/eval/syzygy
)

# Optimization flags for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -DNDEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -flto -DNDEBUG")
endif()

# Common AVX2 and BMI2 compile flags for NNUE and Bitboards
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mbmi2 -mpopcnt")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mbmi2 -mpopcnt")
add_compile_definitions(USE_AVX2 USE_POPCNT USE_BMI2)

file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
)

list(APPEND SRC_FILES "src/eval/syzygy/tbprobe.c")

# Do not include example files from nnue-probe if any
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/eval/nnue/example.cpp")

list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/core/magics_generator.cpp")
add_executable(pawnGO 
    ${SRC_FILES}
)

add_custom_command(TARGET pawnGO POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/src/eval/nnue/nn-62ef826d1a6d.nnue
    ${CMAKE_BINARY_DIR}/nn-62ef826d1a6d.nnue
)
