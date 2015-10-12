#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <dake/math/matrix.hpp>

#include <chrono>
#include <string>
#include <vector>

#include "aurora.hpp"
#include "json-structs.hpp"
#include "ship.hpp"
#include "software.hpp"
#include "ui.hpp"


struct WorldState {
    void initialize(const std::string &scenario);

    ShipState &spawn_ship(const Ship *type);

    std::chrono::system_clock::time_point timestamp, real_timestamp;
    float interval, real_interval;

    dake::math::vec3 sun_light_dir;
    dake::math::vec3 moon_pos;
    float earth_angle;
    float moon_angle_to_sun;

    dake::math::mat4 earth_mv, earth_inv_mv;

    std::vector<ShipState> ships;
    int player_ship;

    std::vector<Aurora> auroras;
    Aurora::HotspotList aurora_hotspots;

    Software *scenario;
    bool scenario_initialized;
};


void do_physics(WorldState &output, const WorldState &input, const Input &user_input);

void enable_player_physics(bool state);
void fix_player_to_ground(bool state);

#endif
