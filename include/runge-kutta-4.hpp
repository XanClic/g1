#ifndef RUNGE_KUTTA_4_HPP
#define RUNGE_KUTTA_4_HPP

#include <dake/math/matrix.hpp>
#include <functional>


struct RK4State {
    RK4State(const dake::math::vec<3, double> &pos,
             const dake::math::vec3 &vel):
        x(pos), v(vel)
    {}

    dake::math::vec<3, double> x;
    dake::math::vec3 v;
};


typedef std::function<dake::math::vec3(const RK4State &state)>
        RK4CalcAccelerationFunc;

RK4State rk4_integrate(const RK4State &initial, float dt,
                       RK4CalcAccelerationFunc calc_accel);

#endif
