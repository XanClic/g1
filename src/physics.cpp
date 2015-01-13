#include <chrono>
#include <cstdio>
#include <ctime>

#include <dake/math.hpp>

#include "options.hpp"
#include "physics.hpp"
#include "software.hpp"


using namespace dake::math;


static inline float time_interval(std::chrono::system_clock::duration d)
{
    return std::chrono::duration_cast<std::chrono::duration<float>>(d).count();
}


void do_physics(WorldState &output, const WorldState &input, const Input &user_input)
{
    output.timestamp = std::chrono::steady_clock::now();
    output.interval  = time_interval(output.timestamp - input.timestamp);

    output.virtual_timestamp = std::chrono::system_clock::now();


    bool ship_list_changed = input.ships.size() != output.ships.size();
    for (size_t i = 0; !ship_list_changed && (i < input.ships.size()); i++) {
        // FIXME
        ship_list_changed = input.ships[i].ship != output.ships[i].ship;
    }

    if (ship_list_changed) {
        output.ships = input.ships;
        output.player_ship = input.player_ship;
    }


    ShipState &player = output.ships[output.player_ship];
    execute_software(player, user_input);


    for (size_t i = 0; i < input.ships.size(); i++) {
        const ShipState &in = input.ships[i];
        ShipState &out = output.ships[i];

        // Positive Z is backwards
        mat3 local_mat(in.right, in.up, -in.forward);

        vec3 forces = vec3::zero(), torque = vec3::zero();
        for (size_t j = 0; j < in.ship->thrusters.size(); j++) {
            float state = in.thruster_states[j];
            if (state < 0.f) {
                state = 0.f;
            } else if (state > 1.f) {
                state = 1.f;
            }

            vec3 force = state * (local_mat * in.ship->thrusters[j].force);

            forces += force;
            torque += (local_mat * in.ship->thrusters[j].relative_position).cross(force);
        }

        forces += -1.e3 * vec<3, double>(in.position) *
                  6.67384e-11 * in.total_mass * 5.974e24
                  / pow(1.e3 * in.position.length(), 3.);

        out.acceleration = forces / in.total_mass;
        out.torque       = torque;

        out.velocity = in.velocity + in.acceleration * output.interval;
        out.position = in.position + in.velocity * 1e-3f * output.interval;

        out.angular_momentum    = in.angular_momentum + out.torque * output.interval;
        out.rotational_velocity = out.angular_momentum / in.total_mass;

        if (in.rotational_velocity.length()) {
            mat3 rot_mat(mat4::identity().rotated(in.rotational_velocity.length() * output.interval,
                                                  in.rotational_velocity));
            out.right   = rot_mat * in.right;
            out.up      = rot_mat * in.up;
            out.forward = rot_mat * in.forward;

            out.right = out.forward.cross(out.up);
            out.up    = out.right.cross(out.forward);

            out.right.normalize();
            out.up.normalize();
            out.forward.normalize();
        } else {
            out.right   = in.right;
            out.up      = in.up;
            out.forward = in.forward;
        }
    }


    // note that this program's timezone is UTC

    time_t time_t_now = std::chrono::system_clock::to_time_t(output.virtual_timestamp);
    // 0 == vernal point (spring)
    float year_angle = (gmtime(&time_t_now)->tm_yday - 79) / 365.f * 2.f * M_PIf;

    output.sun_light_dir = -vec3(cosf(year_angle), 0.f, sinf(year_angle));

    // 2015-04-18 18:56:56
    auto new_moon = std::chrono::system_clock::from_time_t(1429379816);
    float moon_phase_time = time_interval(output.virtual_timestamp - new_moon);

    float moon_phase_angle = fmodf(moon_phase_time / 2551442.9f, 1.f) * 2.f * M_PIf;

    // FIXME: Perigee, apogee and vertical angle (against ecliptic)
    output.moon_pos = 383397.7916f * vec3(cosf(year_angle + moon_phase_angle),
                                          0.f,
                                          sinf(year_angle + moon_phase_angle));

    output.moon_angle_to_sun = moon_phase_angle;

    tm last_midnight_tm = *gmtime(&time_t_now);
    last_midnight_tm.tm_hour = last_midnight_tm.tm_min = last_midnight_tm.tm_sec = 0;

    auto last_midnight = std::chrono::system_clock::from_time_t(mktime(&last_midnight_tm));
    float second_of_day = time_interval(output.virtual_timestamp - last_midnight);

    output.earth_angle  = second_of_day / 86400.f * 2.f * M_PIf;
    output.earth_angle -= year_angle;


    if (global_options.aurora) {
        if (!output.auroras.size()) {
            output.auroras.resize(input.auroras.size());
        }

        for (size_t i = 0; i < output.auroras.size(); i++) {
            output.auroras[i].step(input.auroras[i], input.aurora_hotspots, output);
        }

        output.aurora_hotspots.step(input.aurora_hotspots, output);
    }
}


void WorldState::initialize(void)
{
    timestamp = std::chrono::steady_clock::now();
    interval  = 1.f / 60.f;

    ships.resize(1);
    player_ship = 0;

    ships[0].ship = new Ship;
    ships[0].ship->empty_mass = 500.f;

    ships[0].ship->thrusters.resize(13);
    ships[0].ship->thrusters[ 0] = Thruster(Thruster::MAIN, Thruster::BACK,  Thruster::FORWARD,
                                            vec3( 0.f, 0.f,  2.f), vec3(  0.f,   0.f, -1e7f));
    ships[0].ship->thrusters[ 1] = Thruster(Thruster::RCS,  Thruster::FRONT, Thruster::UP,
                                            vec3( 0.f, 0.f, -2.f), vec3(  0.f,  50.f,   0.f));
    ships[0].ship->thrusters[ 2] = Thruster(Thruster::RCS,  Thruster::FRONT, Thruster::DOWN,
                                            vec3( 0.f, 0.f, -2.f), vec3(  0.f, -50.f,   0.f));
    ships[0].ship->thrusters[ 3] = Thruster(Thruster::RCS,  Thruster::FRONT, Thruster::RIGHT,
                                            vec3( 0.f, 0.f, -2.f), vec3( 50.f,   0.f,   0.f));
    ships[0].ship->thrusters[ 4] = Thruster(Thruster::RCS,  Thruster::FRONT, Thruster::LEFT,
                                            vec3( 0.f, 0.f, -2.f), vec3(-50.f,   0.f,   0.f));
    ships[0].ship->thrusters[ 5] = Thruster(Thruster::RCS,  Thruster::BACK,  Thruster::UP,
                                            vec3( 0.f, 0.f,  2.f), vec3(  0.f,  50.f,   0.f));
    ships[0].ship->thrusters[ 6] = Thruster(Thruster::RCS,  Thruster::BACK,  Thruster::DOWN,
                                            vec3( 0.f, 0.f,  2.f), vec3(  0.f, -50.f,   0.f));
    ships[0].ship->thrusters[ 7] = Thruster(Thruster::RCS,  Thruster::BACK,  Thruster::RIGHT,
                                            vec3( 0.f, 0.f,  2.f), vec3( 50.f,   0.f,   0.f));
    ships[0].ship->thrusters[ 8] = Thruster(Thruster::RCS,  Thruster::BACK,  Thruster::LEFT,
                                            vec3( 0.f, 0.f,  2.f), vec3(-50.f,   0.f,   0.f));
    ships[0].ship->thrusters[ 9] = Thruster(Thruster::RCS,  Thruster::RIGHT, Thruster::UP,
                                            vec3( 2.f, 0.f,  0.f), vec3(  0.f,  50.f,   0.f));
    ships[0].ship->thrusters[10] = Thruster(Thruster::RCS,  Thruster::RIGHT, Thruster::DOWN,
                                            vec3( 2.f, 0.f,  0.f), vec3(  0.f, -50.f,   0.f));
    ships[0].ship->thrusters[11] = Thruster(Thruster::RCS,  Thruster::LEFT,  Thruster::UP,
                                            vec3(-2.f, 0.f,  0.f), vec3(  0.f,  50.f,   0.f));
    ships[0].ship->thrusters[12] = Thruster(Thruster::RCS,  Thruster::LEFT,  Thruster::DOWN,
                                            vec3(-2.f, 0.f,  0.f), vec3(  0.f, -50.f,   0.f));

    ships[0].position     = vec3(0.f, 1.f, 6450.f);
    ships[0].velocity     = vec3(7862.f, 0.f, 0.f);
    ships[0].acceleration = vec3::zero();

    ships[0].rotational_velocity = vec3::zero();
    ships[0].angular_momentum    = vec3::zero();
    ships[0].torque              = vec3::zero();

    ships[0].right   = vec3(0.f, 0.f, 1.f);
    ships[0].up      = vec3(0.f, 1.f, 0.f);
    ships[0].forward = vec3(1.f, 0.f, 0.f);

    ships[0].total_mass = ships[0].ship->empty_mass;

    ships[0].thruster_states.resize(13);
    memset(ships[0].thruster_states.data(), 0, 13 * sizeof(float));

    if (global_options.aurora) {
        auroras.resize(3);
    }
}
