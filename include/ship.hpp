#ifndef SHIP_HPP
#define SHIP_HPP

#include <dake/math/matrix.hpp>

#include "json-structs.hpp"
#include "radar.hpp"


struct ShipState {
    ShipState(const Ship *ship_type);

    const Ship *ship;

    uint64_t id;

    // position in km, velocity in m/s, acceleration in m/s^2
    dake::math::vec<3, double> position;
    dake::math::vec3 velocity, acceleration;
    // rotational_velocity in 1/s, angular_momentum in kg*m^2/s, torque in kg*m^2/s^2 (Nm)
    dake::math::vec3 rotational_velocity, angular_momentum, torque;
    // normalized
    dake::math::vec3 forward, up, right;

    // Can only be applied during the next frame
    dake::math::vec3 weapon_force, weapon_torque;

    dake::math::vec3 local_velocity, local_acceleration;
    dake::math::vec3 local_rotational_velocity;

    // kg
    float total_mass;

    std::vector<float> thruster_states;
    std::vector<float> weapon_cooldowns;

    Radar radar;
};

#endif
