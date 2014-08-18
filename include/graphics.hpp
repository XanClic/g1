#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <dake/math/matrix.hpp>

#include "physics.hpp"


struct GraphicsStatus {
    dake::math::vec3 camera_position;
    dake::math::mat4 world_to_camera;
    dake::math::mat4 projection;

    unsigned width, height;
    float z_near, z_far;
    float yfov;
};


void init_graphics(void);

void set_resolution(unsigned width, unsigned height);
void register_resize_handler(void (*rh)(unsigned w, unsigned h));

void do_graphics(const WorldState &input);

#endif
