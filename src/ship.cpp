#include <dake/math/matrix.hpp>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <climits>

#include "json-structs.hpp"
#include "ship.hpp"
#include "conversion.hpp"

using namespace dake::math;


static uint64_t id_counter;


ShipState::ShipState(const Ship *ship_type, SharedPointer<PhysicsEngine> physicsEngine):
    ship(ship_type)
{
    assert(id_counter < UINT64_MAX);

    // TODO< set position and velocity >
    Eigen::Matrix<double, 3, 3> InertiaTensor;
    physicsBody = SharedPointer<PhysicsBody>(new PhysicsBody(InertiaTensor));
    physicsEngine->addPhysicsBody(physicsBody);

    id = id_counter++;

    //acceleration    = vec3::zero();

    //rotational_velocity = vec3::zero();
    //angular_momentum    = vec3::zero();
    //torque              = vec3::zero();

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

    hull_hitpoints = ship->hull_hitpoints;
}

void ShipState::recalcAndCacheOrbitNormal() {
    // TODO< position is the relative position to the heaviest celestial object which is heavier than a asteroid >
    cachedOrbitNormal = conversion::fromEigenToDake(physicsBody->getLinearVelocity()).cross(conversion::fromEigenToDake(physicsBody->getPosition())).normalized();
}

void ShipState::deal_damage(float amount)
{
    hull_hitpoints -= amount;
    if (hull_hitpoints < 0.f) {
        hull_hitpoints = 0.f;
    }
}
