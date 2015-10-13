#include <dake/math/matrix.hpp>

#include <cstring>

#include "json-structs.hpp"
#include "ship.hpp"


using namespace dake::math;


ShipState::ShipState(const Ship *ship_type):
    ship(ship_type)
{
    position        = vec3::zero();
    velocity        = vec3::zero();
    acceleration    = vec3::zero();

    rotational_velocity = vec3::zero();
    angular_momentum    = vec3::zero();
    torque              = vec3::zero();

    forward = vec3(0.f, 0.f, -1.f);
    up      = vec3(0.f, 1.f,  0.f);
    right   = vec3(1.f, 0.f,  0.f);

    weapon_force    = vec3::zero();
    weapon_torque   = vec3::zero();

    local_velocity      = vec3::zero();
    local_acceleration  = vec3::zero();

    local_rotational_velocity = vec3::zero();

    total_mass = ship->empty_mass;

    thruster_states.resize(ship->thrusters.size(), 0.f);
    weapon_cooldowns.resize(ship->weapons.size(), 0.f);
}
