#ifndef PHYSICS_HPP
#define PHYSICS_HPP

#include <chrono>


struct WorldState {
    void initialize(void);

    std::chrono::steady_clock::time_point timestamp;
    float interval;
};


void do_physics(WorldState &output, const WorldState &input);

#endif
