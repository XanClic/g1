#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <dake/math/fmatrix.hpp>
#include <dake/gl/vertex_array.hpp>

#include "physics.hpp"


struct GraphicsStatus {
    dake::math::fvec3d camera_position;
    dake::math::fvec3 camera_forward;

    dake::math::fmat4 world_to_camera, relative_to_camera;
    dake::math::fmat4 projection;

    unsigned width, height;
    float z_near, z_far;
    float yfov, aspect;
    int time_speed_up;

    float luminance;
};


void init_graphics(void);
void init_game_graphics(void);

void set_resolution(unsigned width, unsigned height);
void register_resize_handler(void (*rh)(unsigned w, unsigned h));

void do_graphics(const WorldState &input);


extern dake::gl::vertex_array *quad_vertices;

#endif
