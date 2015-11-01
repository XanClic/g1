#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>

#include <dake/math.hpp>

#include "generic-data.hpp"
#include "json.hpp"
#include "options.hpp"
#include "physics.hpp"
#include "runge-kutta-4.hpp"
#include "ship_types.hpp"
#include "software.hpp"
#include "weapons.hpp"


using namespace dake::math;


static bool player_physics_enabled = false;
static bool player_fixed_to_ground = false;
static float fixed_to_ground_length = 0.f; // FIXME


static inline float time_interval(const std::chrono::system_clock::duration &d)
{
    using namespace std::chrono;

    return duration_cast<duration<float>>(d).count();
}


static inline std::chrono::system_clock::duration
    interval_duration(float interval)
{
    using namespace std::chrono;

    return duration_cast<system_clock::duration>(duration<float>(interval));
}


static void handle_weapons(WorldState &output, const WorldState &input,
                           const Input &user_input,
                           ShipState &ship_out, const ShipState &ship_in,
                           fvec3 *weapon_force, fvec3 *weapon_torque)
{
    int weapon_count = static_cast<int>(ship_in.ship->weapons.size());
    bool is_player_ship = &ship_in == &input.ships[input.player_ship];
    bool local_mat_initialized = false;
    fmat3 local_mat;

    if (is_player_ship) {
        float aim_xp, aim_xn, aim_yp, aim_yn;
        aim_xp = user_input.get_mapping("aim.+x");
        aim_xn = user_input.get_mapping("aim.-x");
        aim_yp = user_input.get_mapping("aim.+y");
        aim_yn = user_input.get_mapping("aim.-y");

        for (int i = 0; i < weapon_count; i++) {
            ship_out.weapon_forwards[i] = fvec3(aim_xp - aim_xn,
                                                aim_yp - aim_yn,
                                                -1.f).approx_normalized();
        }
    }

    for (int i = 0; i < weapon_count; i++) {
        bool fire = false;

        float new_cooldown = ship_in.weapon_cooldowns[i] - output.interval;

        if (new_cooldown <= 0.f) {
            fire = is_player_ship && user_input.get_mapping("main_fire");
            if (fire) {
                WeaponType wt = ship_in.ship->weapons[i].type;
                const WeaponClass *wc = weapon_classes[wt];

                if (!local_mat_initialized) {
                    local_mat[0] = ship_out.right;
                    local_mat[1] = ship_out.up;
                    local_mat[2] = -ship_out.forward;

                    local_mat_initialized = true;
                }

                fvec3 fwd(local_mat * ship_out.weapon_forwards[i]);

                spawn_particle(output, ship_out, ship_out.position,
                               fvec3(ship_out.velocity +
                                     fwd * wc->projectile_velocity),
                               fwd * 20.f);

                new_cooldown += wc->cooldown;
            }
        }

        if (new_cooldown < 0.f) {
            new_cooldown = 0.f;
        }

        ship_out.weapon_cooldowns[i] = new_cooldown;
        ship_out.weapon_fired[i] = fire;
    }

    // TODO
    *weapon_force = fvec3::zero();
    *weapon_torque = fvec3::zero();
}


void do_physics(WorldState &output, const WorldState &input, const Input &user_input)
{
    output.real_timestamp = std::chrono::system_clock::now();
    output.real_interval  = time_interval(output.real_timestamp - input.real_timestamp);

    output.time_speed_up = input.time_speed_up;

    if (user_input.get_mapping("time_acceleration") &&
        output.time_speed_up < 1000)
    {
        output.time_speed_up *= 10;
    }
    if (user_input.get_mapping("time_deceleration") &&
        output.time_speed_up > 1)
    {
        output.time_speed_up /= 10;
    }

    float time_accel = static_cast<float>(output.time_speed_up);
    if (user_input.get_mapping("pause")) {
        time_accel = 0.f;
    }

    float limited_real_interval = output.real_interval;
    if (limited_real_interval > .1f) {
        limited_real_interval = .1f;
    }

    output.interval  = limited_real_interval * time_accel;
    output.timestamp = input.timestamp + interval_duration(output.interval);

    output.scenario = input.scenario;


    // note that this program's timezone is UTC

    time_t time_t_now = std::chrono::system_clock::to_time_t(output.timestamp);
    // 0 == vernal point (spring)
    float year_angle = (gmtime(&time_t_now)->tm_yday - 79) / 365.f * 2.f * M_PIf;

    output.sun_light_dir = -fvec3(cosf(year_angle), 0.f, sinf(year_angle));

    // 2015-04-18 18:56:56
    auto new_moon = std::chrono::system_clock::from_time_t(1429379816);
    float moon_phase_time = time_interval(output.timestamp - new_moon);

    float moon_phase_angle = fmodf(moon_phase_time / 2551442.9f, 1.f) * 2.f * M_PIf;

    // FIXME: Perigee, apogee and vertical angle (against ecliptic)
    output.moon_pos = 383397.7916f * fvec3(cosf(year_angle + moon_phase_angle),
                                           0.f,
                                           sinf(year_angle + moon_phase_angle));

    output.moon_angle_to_sun = moon_phase_angle;

    tm last_midnight_tm = *gmtime(&time_t_now);
    last_midnight_tm.tm_hour = last_midnight_tm.tm_min = last_midnight_tm.tm_sec = 0;

    auto last_midnight = std::chrono::system_clock::from_time_t(mktime(&last_midnight_tm));
    float second_of_day = time_interval(output.timestamp - last_midnight);

    output.earth_angle  = second_of_day / 86164.09f * 2.f * M_PIf;
    output.earth_angle -= year_angle;

    output.earth_mv = fmat4::scaling(fvec3(6371e3f, 6371e3f, 6371e3f))
                             .rotated_normalized(.41f, fvec3(1.f, 0.f, 0.f))
                             .rotated_normalized(output.earth_angle,
                                                 fvec3(0.f, 1.f, 0.f));
    output.earth_inv_mv = output.earth_mv.inverse();


    if (input.ship_list_changed) {
        output.ships = input.ships;
        output.player_ship = input.player_ship;
        output.ship_list_changed = false;
    }


    ShipState &player = output.ships[output.player_ship];
    execute_flight_control_software(player, user_input, output.interval);


    for (const ShipState &in: input.ships) {
        int i = &in - input.ships.data();
        ShipState &out = output.ships[i];

        // Positive Z is backwards
        fmat3 local_mat(in.right, in.up, -in.forward);


        bool physics_enabled = player_physics_enabled || i != input.player_ship;

        if (physics_enabled) {
            fvec3 forces = fvec3::zero(), torque = fvec3::zero();
            for (size_t j = 0; j < out.ship->thrusters.size(); j++) {
                float state = out.thruster_states[j];
                if (state < 0.f) {
                    state = 0.f;
                } else if (state > 1.f) {
                    state = 1.f;
                }

                fvec3 force = state * (local_mat * out.ship->thrusters[j].force);

                forces += force;
                torque += fvec3(local_mat *
                                out.ship->thrusters[j].relative_position)
                          .cross(force);
            }

            forces += in.weapon_force;
            torque += in.weapon_torque;

            fvec3 accel = forces / in.total_mass;

            auto rk4_calc_accel = [&accel](const RK4State &state) -> fvec3 {
                fvec3 total_accel = accel;
                if (state.x.length() > 6371e3) {
                    total_accel += state.x * (-6.67384e-11 * 5.974e24)
                                   / pow(state.x.length(), 3.);
                }
                return total_accel;
            };

            RK4State rk4_initial(in.position, in.velocity);
            RK4State rk4s = rk4_integrate(rk4_initial, output.interval,
                                          rk4_calc_accel);

            out.acceleration = (rk4s.v - in.velocity) / output.interval;
            out.torque       = torque;

            out.velocity = rk4s.v;
            out.position = rk4s.x;
        } else {
            if (player_fixed_to_ground) {
                fvec3 tangent = crossp(fvec3(input.earth_mv * fvec4(0.f, 1.f, 0.f, 0.f)),
                                       fvec3(in.position)).normalized();
                fvec3 sphere_pos = (input.earth_inv_mv *
                                    fvec4::direction(in.position).normalized());
                sphere_pos.y() = 0.f;
                fvec3 earth_velocity = sphere_pos.length() * 6371e3f * 2.f * M_PIf / 86164.09f * tangent;

                out.velocity = earth_velocity;

                if (!fixed_to_ground_length) {
                    fixed_to_ground_length = in.position.length();
                }
                out.position = output.earth_mv *
                               (input.earth_inv_mv *
                                fvec4::direction(in.position));
                out.position = out.position.normalized() * fixed_to_ground_length;
            } else {
                out.velocity = fvec3d::zero();
                out.position = in.position;
            }

            out.acceleration = fvec3::zero();
            out.torque = fvec3::zero();
        }

        if (!physics_enabled) {
            // FIXME if player_fixed_to_ground
            out.forward = in.forward;
            out.right   = in.right;
            out.up      = in.up;
        }

        if (!physics_enabled && input.scenario_initialized) {
            input.scenario->sub<ScenarioScript>().execute(output, input, user_input);
        }

        if (physics_enabled && out.position.length() < 6371e3f) {
            fvec3 earth_normal = out.position.normalized();
            out.velocity = .8 * (out.velocity -
                                 2. * out.velocity.dot(earth_normal) *
                                fvec3d(earth_normal));
            out.position = 6371e3 / out.position.length() * out.position;
        }

        out.orbit_normal = out.velocity.cross(out.position).normalized();

        if (physics_enabled) {
            out.angular_momentum    = in.angular_momentum + out.torque * output.interval;
            out.rotational_velocity = out.angular_momentum / in.total_mass;
        } else {
            out.angular_momentum    = fvec3::zero();
            out.rotational_velocity = fvec3::zero();
        }

        if (physics_enabled) {
            if (in.rotational_velocity.length()) {
                fmat3 rot_mat(fmat3::rotation(in.rotational_velocity.length()
                                              * output.interval,
                                              in.rotational_velocity));
                // out.right   = (rot_mat * in.right).normalized();
                out.up      = (rot_mat * in.up).normalized();
                out.forward = (rot_mat * in.forward).normalized();

                out.right = out.forward.cross(out.up);
                out.up    = out.right.cross(out.forward);
            } else {
                out.right   = in.right;
                out.up      = in.up;
                out.forward = in.forward;
            }
        }

        // .transpose() == .invert() (local_mat is a rotation matrix)
        local_mat.transpose();
        out.local_velocity            = local_mat * out.velocity;
        out.local_acceleration        = local_mat * out.acceleration;
        out.local_rotational_velocity = local_mat * out.rotational_velocity;
        out.local_orbit_normal        = local_mat * out.orbit_normal;

        out.hull_hitpoints = in.hull_hitpoints;


        handle_weapons(output, input, user_input, out, in,
                       &out.weapon_force, &out.weapon_torque);
    }


    handle_particles(output.particles, input.particles, output, player);


    for (const ShipState &in: input.ships) {
        ShipState &out = output.ships[&in - input.ships.data()];

        out.radar.update(in.radar, out, output, user_input);
    }


    for (auto it = output.ships.begin(); it != output.ships.end();) {
        if (it->hull_hitpoints <= 0.f) {
            it = output.ships.erase(it);
            output.ship_list_changed = true;
        } else {
            ++it;
        }
    }


    if (global_options.aurora) {
        if (!output.auroras.size()) {
            output.auroras.resize(input.auroras.size());
        }

        for (size_t i = 0; i < output.auroras.size(); i++) {
            output.auroras[i].step(input.auroras[i], input.aurora_hotspots, output);
        }

        output.aurora_hotspots.step(input.aurora_hotspots, output);
    }


    if (!input.scenario_initialized) {
        output.scenario->sub<ScenarioScript>().initialize(output);
    }
    output.scenario_initialized = true;
}


void WorldState::initialize(const std::string &sn)
{
    real_timestamp = std::chrono::system_clock::now();
    timestamp      = std::chrono::system_clock::now();

    time_speed_up = 1;

    scenario = get_scenario(sn);
    if (!scenario) {
        throw std::runtime_error("Unknown scenario " + sn + " specified");
    }
    scenario_initialized = false;

    spawn_ship(ship_types["mumeifune"]);
    player_ship = 0;

    if (global_options.aurora) {
        auroras.resize(3);
    }
}


ShipState &WorldState::spawn_ship(const Ship *type)
{
    ships.emplace_back(type);
    ship_list_changed = true;
    return ships.back();
}


void enable_player_physics(bool state)
{
    player_physics_enabled = state;
}


void fix_player_to_ground(bool state)
{
    player_fixed_to_ground = state;
    fixed_to_ground_length = 0.f;
}
