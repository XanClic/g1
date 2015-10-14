#pragma once

template<typename vectortype>
struct RungeKutta4State {
  vectortype x;      // position
  vectortype v;      // velocity
};

// needs to be implemented by the physics simulation which uses the RungeKutta4 solver
template<typename vectortype>
class IAcceleration {
  public:
  virtual vectortype calculateAcceleration(const RungeKutta4State<vectortype> &state, const float time) const = 0;
};

// see http://gafferongames.com/game-physics/integration-basics/
template<typename vectortype>
class RungeKutta4 {
public:
  
  struct Derivative {
    vectortype dx;      // dx/dt = velocity
    vectortype dv;      // dv/dt = acceleration
  };
  
  Derivative evaluate( const RungeKutta4State<vectortype> &initial, float t, float dt, const Derivative &d ) {
    RungeKutta4State<vectortype> state;
    state.x = initial.x + d.dx*dt;
    state.v = initial.v + d.dv*dt;

    Derivative output;
    output.dx = state.v;
    output.dv = acceleration->calculateAcceleration(state, t+dt);
    return output;
  }
  
  void integrate( RungeKutta4State<vectortype> &state, float t, float dt ) {
    Derivative a,b,c,d;

    a = evaluate( state, t, 0.0f, Derivative() );
    b = evaluate( state, t, dt*0.5f, a );
    c = evaluate( state, t, dt*0.5f, b );
    d = evaluate( state, t, dt, c );

    vectortype dxdt = ( a.dx + (b.dx + c.dx)*2.0f + d.dx ) * (1.0f / 6.0f);
    vectortype dvdt = ( a.dv + (b.dv + c.dv)*2.0f + d.dv ) * (1.0f / 6.0f);

    state.x = state.x + dxdt * dt;
    state.v = state.v + dvdt * dt;
  }
  
  IAcceleration<vectortype> *acceleration;
};
