cmake_minimum_required(VERSION 3.16)
project(Wanda CXX)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 20)
include(FetchContent)
include(cmake/FetchLagom.cmake)

add_compile_options(-Werror=implicit-fallthrough)
add_compile_options(-Werror=switch)
add_compile_options(-Wno-literal-suffix)
add_compile_options(-fno-gnu-keywords)

add_subdirectory(LibSourceEngine)
add_subdirectory(Server)

add_executable(introspect
        introspect.cpp
        )

target_include_directories(introspect SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        )

target_link_libraries(introspect PRIVATE Lagom::Core Lagom::Main SourceEngine)
add_executable(vpktool
        vpktool.cpp
        )

target_include_directories(vpktool SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_BINARY_DIR}
        )

target_link_libraries(vpktool PRIVATE Lagom::Core Lagom::Main SourceEngine)