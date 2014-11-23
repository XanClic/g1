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
    dake_source
    GIT_REPOSITORY git://github.com/XanClic/dake.git
    GIT_TAG origin/master
    SOURCE_DIR dake
    CMAKE_COMMAND true
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
    BINARY_DIR dake
    INSTALL_COMMAND true
)

ExternalProject_Get_Property(dake_source binary_dir)

add_library(dake IMPORTED STATIC)
add_dependencies(dake dake_source)
set_property(TARGET dake APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(dake PROPERTIES IMPORTED_LOCATION_NOCONFIG ${binary_dir}/libdake.a)

include_directories(include ${binary_dir}/include ${SDL2_INCLUDE_DIR} ${LUA_INCLUDE_DIR})
link_directories(${binary_dir})

add_executable(g1 src/main.cpp src/ui.cpp src/main_loop.cpp src/graphics.cpp src/physics.cpp src/environment.cpp src/cockpit.cpp src/xsmd.cpp src/software.cpp src/localize.cpp src/text.cpp src/menu.cpp src/aurora.cpp)
target_link_libraries(g1 ${OPENGL_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${SDL2_LIBRARIES} ${LUA_LIBRARIES} txc_dxtn dake)

add_executable(create-sphere tools/create-sphere.cpp)
target_link_libraries(create-sphere dake)

add_executable(polar-to-cube tools/polar-to-cube.cpp)
target_link_libraries(polar-to-cube ${PNG_LIBRARIES} ${JPEG_LIBRARIES} txc_dxtn dake)

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g2 -Wall -Wextra -Wshadow")
