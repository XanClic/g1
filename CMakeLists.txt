cmake_minimum_required(VERSION 2.8.8)

include(ExternalProject)
include(FindPkgConfig)

project(g1)

find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(Lua51 REQUIRED)

pkg_search_module(SDL2 REQUIRED sdl2)
pkg_search_module(LIBEPOXY REQUIRED epoxy)
pkg_search_module(LIBUSB OPTIONAL libusb-1.0)

find_library(LIBTXC_DXTN txc_dxtn)

if (NOT LIBTXC_DXTN)
    message(FATAL_ERROR "libtxc_dxtn not found!")
endif (NOT LIBTXC_DXTN)

ExternalProject_Add(
    dake_source
    GIT_REPOSITORY git://github.com/XanClic/dake.git
    GIT_TAG origin/master
    SOURCE_DIR dake
    CMAKE_COMMAND true
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} "CXX=${CMAKE_CXX_COMPILER}" libdake.a
    BINARY_DIR dake
    INSTALL_COMMAND true
)

ExternalProject_Get_Property(dake_source binary_dir)
set(dake_dir "${binary_dir}")

add_library(dake IMPORTED STATIC)
add_dependencies(dake dake_source)
set_property(TARGET dake APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(dake PROPERTIES IMPORTED_LOCATION_NOCONFIG "${binary_dir}/libdake.a")

add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/include"
                   COMMAND mkdir -p "${CMAKE_BINARY_DIR}/include")

add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/include/json-structs.hpp" "${CMAKE_BINARY_DIR}/serializer.cpp"
                   COMMAND "${CMAKE_SOURCE_DIR}/tools/generate-serializer.rb" "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}"
                   DEPENDS json-structs/* json-enums/* "${CMAKE_BINARY_DIR}/include")

include_directories(include "${CMAKE_BINARY_DIR}/include" "${dake_dir}/include"
                    ${SDL2_INCLUDE_DIRS} ${LUA_INCLUDE_DIR}
                    ${LIBUSB_INCLUDE_DIRS} ${LIBEPOXY_INCLUDE_DIRS})
link_directories("${dake_dir}")

if (LIBUSB_FOUND)
    set(SC_FILES src/steam_controller.cpp)
    set(SC_CXXFLAGS -DHAS_LIBUSB)
else (LIBUSB_FOUND)
    set(SC_FILES)
    set(SC_CXXFLAGS)
endif (LIBUSB_FOUND)

add_executable(g1 src/main.cpp src/ui.cpp src/main_loop.cpp src/graphics.cpp
                  src/physics.cpp src/environment.cpp src/cockpit.cpp
                  src/gltf.cpp src/software.cpp src/localize.cpp src/text.cpp
                  src/menu.cpp src/aurora.cpp src/generic-data.cpp src/json.cpp
                  src/ship_types.cpp src/ship.cpp src/weapons.cpp
                  src/particles.cpp src/runge-kutta-4.cpp src/radar.cpp
                  ${SC_FILES}
                  "${CMAKE_BINARY_DIR}/serializer.cpp"
                  "${CMAKE_BINARY_DIR}/include/json-structs.hpp")

target_link_libraries(g1 dake ${OPENGL_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES}
                         ${SDL2_LIBRARIES} ${LUA_LIBRARIES} ${LIBUSB_LIBRARIES}
                         ${LIBTXC_DXTN} ${LIBEPOXY_LIBRARIES} m)

add_executable(create-sphere tools/create-sphere.cpp)
target_link_libraries(create-sphere dake m)

add_executable(polar-to-cube tools/polar-to-cube.cpp)
target_link_libraries(polar-to-cube dake ${PNG_LIBRARIES} ${JPEG_LIBRARIES}
                                    txc_dxtn m)

set(CMAKE_CXX_FLAGS "-std=c++11 -O3 -g2 -Wall -Wextra -Wshadow ${SC_CXXFLAGS}")
