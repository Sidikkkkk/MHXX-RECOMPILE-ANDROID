cmake_minimum_required(VERSION 3.22.1)
project(mhxx_native LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Source files
set(CORE_SOURCES
    src/bridge/native_bridge.cpp
    src/hle/ctr_os.cpp
    src/recompiler/recompiler.cpp
)

# Build shared library for Android NDK
add_library(mhxx_native SHARED ${CORE_SOURCES})

# Target Android native system libraries
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(mhxx_native
    PRIVATE
    ${log-lib}
    ${android-lib}
)

