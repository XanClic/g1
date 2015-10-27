#ifndef RUNGE_KUTTA_4_HPP
#define RUNGE_KUTTA_4_HPP

#include <dake/math/fmatrix.hpp>
#include <functional>


struct RK4State {
    RK4State(const dake::math::fvec3d &pos,
             const dake::math::fvec3d &vel):
        x(pos), v(vel)
    {}

    dake::math::fvec3d x, v;
};


typedef std::function<dake::math::fvec3(const RK4State &state)>
        RK4CalcAccelerationFunc;

RK4State rk4_integrate(const RK4State &initial, float dt,
                       RK4CalcAccelerationFunc calc_accel);

#endif
