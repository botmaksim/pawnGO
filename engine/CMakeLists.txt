cmake_minimum_required(VERSION 3.20)
project(pawnGO VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto")

include_directories(src src/nnue/src)

# Optimization flags for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -DNDEBUG")
endif()

# Common AVX2 compile flags for nnue-probe
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")

file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
)

# Do not include example files from nnue-probe if any
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/nnue/example.cpp")

list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/magics_generator.cpp")
add_executable(pawnGO 
    ${SRC_FILES}
)
list(REMOVE_ITEM SRC_FILES "${CMAKE_SOURCE_DIR}/src/magics_generator.cpp")
