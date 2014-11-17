cmake_minimum_required(VERSION 2.8.8)

include(ExternalProject)
include(FindPkgConfig)

project(g1)

find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Lua51 REQUIRED)

pkg_search_module(SDL2 REQUIRED sdl2)

ExternalProject_Add(
    dake
    GIT_REPOSITORY git://github.com/XanClic/dake.git
    GIT_TAG origin/master
    SOURCE_DIR dake
    CMAKE_COMMAND true
    BUILD_COMMAND make
    BINARY_DIR dake
    INSTALL_COMMAND true
)

ExternalProject_Get_Property(dake binary_dir)

include_directories(include ${binary_dir}/include ${SDL2_INCLUDE_DIR} ${LUA_INCLUDE_DIR})
link_directories(${binary_dir})

add_executable(g1 src/main.cpp src/ui.cpp src/main_loop.cpp src/graphics.cpp src/physics.cpp src/environment.cpp src/cockpit.cpp src/xsmd.cpp src/software.cpp src/localize.cpp src/text.cpp src/menu.cpp src/aurora.cpp)
add_dependencies(g1 dake)
target_link_libraries(g1 ${OPENGL_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${SDL2_LIBRARIES} ${LUA_LIBRARIES} libdake.a)

add_executable(create-sphere tools/create-sphere.cpp)
add_dependencies(create-sphere dake)
target_link_libraries(create-sphere libdake.a)

add_executable(polar-to-cube tools/polar-to-cube.cpp)
add_dependencies(polar-to-cube dake)
target_link_libraries(polar-to-cube ${PNG_LIBRARIES} ${JPEG_LIBRARIES} libdake.a)

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g2 -Wall -Wextra -Wshadow")
