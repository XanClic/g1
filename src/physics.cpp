#include <chrono>
#include <cstdio>
#include <ctime>

#include <dake/math.hpp>

#include "physics.hpp"


using namespace dake::math;


void do_physics(WorldState &output, const WorldState &input)
{
    output.timestamp = std::chrono::steady_clock::now();
    output.interval  = std::chrono::duration_cast<std::chrono::duration<float>>(output.timestamp - input.timestamp).count();

    output.virtual_timestamp = std::chrono::system_clock::now();


    if (input.player_thrusters.length() < INFINITY) {
        output.player_accel    = input.player_thrusters;
        output.player_velocity = input.player_velocity + input.player_accel    * output.interval;
        output.player_position = input.player_position + input.player_velocity * output.interval;
    } else {
        output.player_accel    = vec3::zero();
        output.player_velocity = vec3::zero();
        output.player_position = input.player_position;
    }

    output.player_right = mat3(mat4::identity().rotated(-output.right / 100.f, input.player_up)) * input.player_right;
    output.player_up    = mat3(mat4::identity().rotated(-output.up / 100.f, output.player_right)) * input.player_up;

    output.player_right.normalize();
    output.player_up.normalize();

    output.player_forward = output.player_right.cross(output.player_up);


    time_t time_t_now = std::chrono::system_clock::to_time_t(output.virtual_timestamp);
    // 0 == vernal point (spring)
    float year_angle = (gmtime(&time_t_now)->tm_yday - 79) / 365.f * 2.f * static_cast<float>(M_PI);

    output.sun_light_dir = -vec3(cosf(year_angle), 0.f, sinf(year_angle));

    tm last_midnight = *gmtime(&time_t_now);
    last_midnight.tm_hour = last_midnight.tm_min = last_midnight.tm_sec = 0;

    float second_of_day = std::chrono::duration_cast<std::chrono::duration<float>>(output.virtual_timestamp - std::chrono::system_clock::from_time_t(mktime(&last_midnight))).count();

    output.earth_angle  = second_of_day / 86400.f * 2.f * static_cast<float>(M_PI);
    output.earth_angle -= year_angle;
}


void WorldState::initialize(void)
{
    timestamp = std::chrono::steady_clock::now();
    interval  = 1.f / 60.f;

    player_forward = vec3( 0.f, 0.f,  1.f);
    player_up      = vec3( 0.f, 1.f,  0.f);
    player_right   = vec3(-1.f, 0.f,  0.f);

    player_position = vec3(0.f, 1.f, 6450.f);
    player_velocity = vec3::zero();
    player_accel    = vec3::zero();
}
