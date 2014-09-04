#ifndef SHIP_HPP
#define SHIP_HPP

#include <dake/math/matrix.hpp>

#include <vector>


struct Thruster {
    dake::math::vec3 force;
    dake::math::vec3 relative_position;

    enum Type {
        RCS,
        MAIN
    } type;

    enum DirectionPosition {
        RIGHT,
        LEFT,
        UP,
        DOWN,
        FORWARD,
        BACKWARD,

        TOP = UP,
        BOTTOM = DOWN,
        FRONT = FORWARD,
        BACK = BACKWARD
    };

    DirectionPosition general_position, general_direction;


    Thruster(void) {}
    Thruster(const Type &t, const DirectionPosition &gen_pos, const DirectionPosition &gen_dir, const dake::math::vec3 &rel_pos, const dake::math::vec3 &f): force(f), relative_position(rel_pos), type(t), general_position(gen_pos), general_direction(gen_dir) {}
};


struct Ship {
    float empty_mass;

    std::vector<Thruster> thrusters;
};


struct ShipState {
    Ship *ship;

    // position in km, velocity in m/s, acceleration in m/s^2
    dake::math::vec3 position, velocity, acceleration;
    // rotational_velocity in 1/s, angular_momentum in kg*m^2/s, torque in kg*m^2/s^2 (Nm)
    dake::math::vec3 rotational_velocity, angular_momentum, torque;
    // normalized
    dake::math::vec3 forward, up, right;

    // kg
    float total_mass;

    std::vector<float> thruster_states;
};

#endif
