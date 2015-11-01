#include <dake/math/fmatrix.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <climits>

#include "json-structs.hpp"
#include "ship.hpp"


using namespace dake::math;


static uint64_t id_counter;


ShipState::ShipState(const Ship *ship_type):
    ship(ship_type)
{
    assert(id_counter < UINT64_MAX);

    id = id_counter++;

    position        = fvec3d::zero();
    velocity        = fvec3d::zero();
    acceleration    = fvec3::zero();

    rotational_velocity = fvec3::zero();
    angular_momentum    = fvec3::zero();
    torque              = fvec3::zero();

    forward = fvec3(0.f, 0.f, -1.f);
    up      = fvec3(0.f, 1.f,  0.f);
    right   = fvec3(1.f, 0.f,  0.f);

    weapon_force    = fvec3::zero();
    weapon_torque   = fvec3::zero();

    local_velocity      = fvec3::zero();
    local_acceleration  = fvec3::zero();

    local_rotational_velocity = fvec3::zero();

    total_mass = ship->empty_mass;

    thruster_states.resize(ship->thrusters.size(), 0.f);
    weapon_cooldowns.resize(ship->weapons.size(), 0.f);
    weapon_fired.resize(ship->weapons.size(), false);
    weapon_forwards.resize(ship->weapons.size(), fvec3(0.f, 0.f, -1.f));

    hull_hitpoints = ship->hull_hitpoints;
}


void ShipState::deal_damage(float amount)
{
    hull_hitpoints -= amount;
    if (hull_hitpoints < 0.f) {
        hull_hitpoints = 0.f;
    }
}
