#include <chrono>
#include <cstdio>

#include <dake/math.hpp>

#include "physics.hpp"


using namespace dake::math;


void do_physics(WorldState &output, const WorldState &input)
{
    output.timestamp = std::chrono::steady_clock::now();
    output.interval  = std::chrono::duration_cast<std::chrono::duration<float>>(output.timestamp - input.timestamp).count();

    output.player_position = input.player_position + input.player_velocity * output.interval;
    output.player_velocity = input.player_velocity + input.player_accel    * output.interval;

    // lel hack
    if (!output.player_accel.length()) {
        output.player_velocity = vec3::zero();
    }

    output.player_right = mat3(mat4::identity().rotated(-output.right / 100.f, input.player_up)) * input.player_right;
    output.player_up    = mat3(mat4::identity().rotated(-output.up / 100.f, output.player_right)) * input.player_up;

    output.player_right.normalize();
    output.player_up.normalize();

    output.player_forward = output.player_right.cross(output.player_up);
}


void WorldState::initialize(void)
{
    timestamp = std::chrono::steady_clock::now();
    interval  = 1.f / 60.f;

    player_forward = vec3( 0.f, 0.f,  1.f);
    player_up      = vec3( 0.f, 1.f,  0.f);
    player_right   = vec3(-1.f, 0.f,  0.f);

    player_position = vec3(0.f, 1.f, 6500.f);
    player_velocity = vec3::zero();
    player_accel    = vec3::zero();
}
