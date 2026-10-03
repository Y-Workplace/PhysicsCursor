cmake_minimum_required(VERSION 3.20)
project(PhysicsCursor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(XCURSOR REQUIRED IMPORTED_TARGET xcursor)

set(SOURCES
    src/main.cpp
    src/Font8x8.cpp
)

set(HEADERS
    src/PhysicsEngine.hpp
    src/CursorRenderer.hpp
    src/HUD.hpp
    src/Font8x8.hpp
)

add_executable(physics_cursor ${SOURCES} ${HEADERS})

target_include_directories(physics_cursor PRIVATE
    ${SDL3_INCLUDE_DIRS}
    src
)

target_link_libraries(physics_cursor PRIVATE
    PkgConfig::SDL3
    PkgConfig::XCURSOR
    m
)

target_compile_options(physics_cursor PRIVATE
    -O3
    -Wall
    -Wextra
    ${SDL3_CFLAGS_OTHER}
)

include(CTest)
if(BUILD_TESTING)
    add_executable(physics_tests tests/physics_tests.cpp)
    target_include_directories(physics_tests PRIVATE src)
    add_test(NAME physics_tests COMMAND /lib64/ld-linux-x86-64.so.2 $<TARGET_FILE:physics_tests>)
endif()
