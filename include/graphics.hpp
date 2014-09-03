#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <dake/math/matrix.hpp>
#include <dake/gl/vertex_array.hpp>

#include "physics.hpp"


struct GraphicsStatus {
    dake::math::vec3 camera_position, camera_forward;
    dake::math::mat4 world_to_camera;
    dake::math::mat4 projection;

    unsigned width, height;
    float z_near, z_far;
    float yfov;

    float luminance;
};


void init_graphics(void);

void set_resolution(unsigned width, unsigned height);
void register_resize_handler(void (*rh)(unsigned w, unsigned h));

void do_graphics(const WorldState &input);


extern dake::gl::vertex_array *quad_vertices;

#endif
