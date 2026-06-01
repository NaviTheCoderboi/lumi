cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME "lumi")
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_DIR src)
set(INCLUDE_DIR "include")
set(PROTOCOL_DIR "protocols")
set(EXTERNAL_DIR "external")

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND REQUIRED wayland-client wayland-egl)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(EPOXY   REQUIRED epoxy)

add_executable(${PROJECT_NAME} ${SRC_DIR}/main.cpp)

file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE HEADER_FILES ${INCLUDE_DIR}/*.h ${INCLUDE_DIR}/*.hpp)
file(GLOB_RECURSE PROTOCOL_SOURCES ${PROTOCOL_DIR}/*.c)
file(GLOB_RECURSE PROTOCOL_HEADERS ${PROTOCOL_DIR}/*.h)

target_sources(${PROJECT_NAME} PRIVATE
    ${SRC_FILES}
    ${HEADER_FILES}
    ${PROTOCOL_SOURCES}
    ${PROTOCOL_HEADERS}
    ${EXTERNAL_DIR}/nanovg/src/nanovg.c
    ${EXTERNAL_DIR}/nanosvg/src/nanosvg.h
    ${EXTERNAL_DIR}/nanosvg/src/nanosvgrast.h
    ${EXTERNAL_DIR}/ryml/ryml.hpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIR}
    ${PROTOCOL_DIR}
    ${EXTERNAL_DIR}/nanovg/src
    ${EXTERNAL_DIR}/nanosvg/src
    ${EXTERNAL_DIR}/ryml
    ${WAYLAND_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${WAYLAND_LIBRARIES}
    ${EGL_LIBRARIES}
    ${EPOXY_LIBRARIES}   
)

# flags for build
target_compile_options(${PROJECT_NAME} PRIVATE
    -fno-exceptions
    -fno-rtti
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:
        -g3
        -O0
        -Wall
        -Wextra
        -Wpedantic
        -fsanitize=address
        -fsanitize=undefined
    >

    $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -march=native
    >
)

target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:
        -fsanitize=address
        -fsanitize=undefined
    >
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:LUMI_DEBUG>
    $<$<CONFIG:Release>:LUMI_RELEASE>
)