#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <dake/math/matrix.hpp>

#include <chrono>


struct WorldState {
    void initialize(void);

    std::chrono::steady_clock::time_point timestamp;
    float interval;

    float right, up;

    dake::math::vec3 player_position, player_velocity, player_accel;
    dake::math::vec3 player_forward, player_up, player_right;
};


void do_physics(WorldState &output, const WorldState &input);

#endif
