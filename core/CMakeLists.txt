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
    src/hle/svc_handler.cpp
    src/hle/ipc_dispatcher.cpp
    src/graphics/gles_renderer.cpp
    src/recompiler/recompiler.cpp
)

# Build shared library for Android NDK
add_library(mhxx_native SHARED ${CORE_SOURCES})

# Target Android native system libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(gles3-lib GLESv3)
find_library(egl-lib EGL)

target_link_libraries(mhxx_native
    PRIVATE
    ${log-lib}
    ${android-lib}
    ${gles3-lib}
    ${egl-lib}
)

