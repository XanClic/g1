#ifndef SHIP_HPP
#define SHIP_HPP

#include <dake/math/matrix.hpp>

#include "json-structs.hpp"


struct ShipState {
    Ship *ship;

    // position in km, velocity in m/s, acceleration in m/s^2
    dake::math::vec3 position, velocity, acceleration;
    // rotational_velocity in 1/s, angular_momentum in kg*m^2/s, torque in kg*m^2/s^2 (Nm)
    dake::math::vec3 rotational_velocity, angular_momentum, torque;
    // normalized
    dake::math::vec3 forward, up, right;

    dake::math::vec3 local_velocity, local_acceleration;
    dake::math::vec3 local_rotational_velocity;

    // kg
    float total_mass;

    std::vector<float> thruster_states;
};

#endif
