#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <dake/math/matrix.hpp>

#include <chrono>
#include <vector>

#include "aurora.hpp"
#include "ship.hpp"
#include "ui.hpp"


struct WorldState {
    void initialize(void);

    std::chrono::steady_clock::time_point timestamp;
    std::chrono::system_clock::time_point virtual_timestamp;
    float interval;

    dake::math::vec3 sun_light_dir;
    dake::math::vec3 moon_pos;
    float earth_angle;
    float moon_angle_to_sun;

    std::vector<ShipState> ships;
    int player_ship;

    std::vector<Aurora> auroras;
    Aurora::HotspotList aurora_hotspots;
};


void do_physics(WorldState &output, const WorldState &input, const Input &user_input);

#endif
