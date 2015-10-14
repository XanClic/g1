#include <dake/math/matrix.hpp>

#include "runge-kutta-4.hpp"


using namespace dake::math;


struct Increments {
    Increments(void): dx(vec3::zero()), dv(vec3::zero()) {}
    Increments(const vec3 &dxdt, const vec3 &dvdt): dx(dxdt), dv(dvdt) {}

    vec3 dx, dv;
};


static Increments evaluate(const RK4State &initial, float dt,
                           const Increments &d,
                           RK4CalcAccelerationFunc calc_accel)
{
    RK4State state(initial.x + d.dx * dt,
                   initial.v + d.dv * dt);

    return Increments(state.v, calc_accel(state));
}


RK4State rk4_integrate(const RK4State &initial, float dt,
                       RK4CalcAccelerationFunc calc_accel)
{
    Increments k1, k2, k3, k4;

    k1 = evaluate(initial, 0.f * dt, Increments(), calc_accel);
    k2 = evaluate(initial, .5f * dt, k1,           calc_accel);
    k3 = evaluate(initial, .5f * dt, k2,           calc_accel);
    k4 = evaluate(initial, 1.f * dt, k3,           calc_accel);

    vec3 dx = (k1.dx + 2.f * (k2.dx + k3.dx) + k4.dx) * (1.f / 6.f);
    vec3 dv = (k1.dv + 2.f * (k2.dv + k3.dv) + k4.dv) * (1.f / 6.f);

    return RK4State(initial.x + dx * dt,
                    initial.v + dv * dt);
}
