#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <dake/math/matrix.hpp>

#include <chrono>


struct WorldState {
    void initialize(void);

    std::chrono::steady_clock::time_point timestamp;
    std::chrono::system_clock::time_point virtual_timestamp;
    float interval;

    float right, up;
    float roll;

    dake::math::vec3 player_position, player_velocity, player_accel;
    dake::math::vec3 player_rot_velocity, player_rot_accel;
    dake::math::vec3 player_forward, player_up, player_right;
    dake::math::vec3 player_thrusters;

    dake::math::vec3 sun_light_dir;
    float earth_angle;
};


void do_physics(WorldState &output, const WorldState &input);

#endif
