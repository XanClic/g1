#include <chrono>
#include <cstdio>

#include "physics.hpp"


void do_physics(WorldState &output, const WorldState &input)
{
    output.timestamp = std::chrono::steady_clock::now();
    output.interval  = std::chrono::duration_cast<std::chrono::duration<float>>(output.timestamp - input.timestamp).count();
}


void WorldState::initialize(void)
{
    timestamp = std::chrono::steady_clock::now();
    interval  = 1.f / 60.f;
}
