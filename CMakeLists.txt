cmake_minimum_required(VERSION 3.10)
project(MAZE_A_MATICS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND CMAKE_PREFIX_PATH
    "$ENV{HOME}/raylib-install"
    "/opt/homebrew"
    "/usr/local"
)

find_package(raylib QUIET)

if(raylib_FOUND)
    if(TARGET raylib::raylib)
        set(RAYLIB_TARGET raylib::raylib)
    elseif(TARGET raylib)
        set(RAYLIB_TARGET raylib)
    else()
        message(FATAL_ERROR "Raylib was found, but it did not provide a usable CMake target.")
    endif()
else()
    find_path(RAYLIB_INCLUDE_DIR raylib.h
        PATHS
            "$ENV{HOME}/raylib-install/include"
            "/opt/homebrew/include"
            "/usr/local/include"
    )
    find_library(RAYLIB_LIBRARY raylib
        PATHS
            "$ENV{HOME}/raylib-install/lib"
            "/opt/homebrew/lib"
            "/usr/local/lib"
    )

    if(NOT RAYLIB_INCLUDE_DIR OR NOT RAYLIB_LIBRARY)
        message(FATAL_ERROR "Raylib was not found. On macOS, install it with: brew install raylib")
    endif()

    add_library(raylib::raylib UNKNOWN IMPORTED)
    set_target_properties(raylib::raylib PROPERTIES
        IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}"
    )
    set(RAYLIB_TARGET raylib::raylib)
endif()

add_executable(maze-a-matics main.cpp)
target_link_libraries(maze-a-matics ${RAYLIB_TARGET})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(maze-a-matics m X11 pthread dl rt)
elseif(APPLE)
    target_link_libraries(maze-a-matics
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
        "-framework CoreVideo"
    )
endif()

target_include_directories(maze-a-matics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
