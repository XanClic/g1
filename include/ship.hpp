#ifndef SHIP_HPP
#define SHIP_HPP

#include <dake/math/fmatrix.hpp>

#include "align-allocator.hpp"
#include "json-structs.hpp"
#include "radar.hpp"


struct ShipState {
    ShipState(const Ship *ship_type);

    void deal_damage(float amount);

    const Ship *ship;

    uint64_t id;

    // position in km, velocity in m/s, acceleration in m/s^2
    dake::math::fvec3d position, velocity;
    dake::math::fvec3 acceleration;
    // rotational_velocity in 1/s, angular_momentum in kg*m^2/s, torque in kg*m^2/s^2 (Nm)
    dake::math::fvec3 rotational_velocity, angular_momentum, torque;
    // normalized
    dake::math::fvec3 forward, up, right;

    dake::math::fvec3 orbit_normal;

    // Can only be applied during the next frame
    dake::math::fvec3 weapon_force, weapon_torque;

    dake::math::fvec3 local_velocity, local_acceleration;
    dake::math::fvec3 local_rotational_velocity;
    dake::math::fvec3 local_orbit_normal;

    // kg
    float total_mass;

    std::vector<float> thruster_states;
    std::vector<float> weapon_cooldowns;
    std::vector<bool> weapon_fired;
    AlignedVector<dake::math::fvec3> weapon_forwards;

    Radar radar;

    float hull_hitpoints;
};

#endif
