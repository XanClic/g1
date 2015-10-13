#include <chrono>
#include <cstdio>
#include <ctime>

#include <dake/math.hpp>

#include "generic-data.hpp"
#include "json.hpp"
#include "options.hpp"
#include "physics.hpp"
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
                           vec3 *weapon_force, vec3 *weapon_torque)
{
    int weapon_count = static_cast<int>(ship_in.ship->weapons.size());
    bool is_player_ship = &ship_in == &input.ships[input.player_ship];

    for (int i = 0; i < weapon_count; i++) {
        float new_cooldown = ship_in.weapon_cooldowns[i] - output.interval;

        if (new_cooldown <= 0.f) {
            bool fire = is_player_ship && user_input.get_mapping("main_fire");

            if (fire) {
                WeaponType wt = ship_in.ship->weapons[i].type;
                const WeaponClass *wc = weapon_classes[wt];

                spawn_particle(output, ship_out.position,
                               ship_out.velocity + ship_out.forward * 1e3f);

                new_cooldown += wc->cooldown;
            }
        }

        if (new_cooldown < 0.f) {
            new_cooldown = 0.f;
        }

        ship_out.weapon_cooldowns[i] = new_cooldown;
    }

    // TODO
    *weapon_force = vec3::zero();
    *weapon_torque = vec3::zero();
}


void do_physics(WorldState &output, const WorldState &input, const Input &user_input)
{
    output.real_timestamp = std::chrono::system_clock::now();
    output.real_interval  = time_interval(output.real_timestamp - input.real_timestamp);

    float time_accel = 1.f + 9.f * user_input.get_mapping("time_acceleration");
    if (user_input.get_mapping("pause")) {
        time_accel = 0.f;
    }

    output.interval  = output.real_interval * time_accel;
    output.timestamp = input.timestamp + interval_duration(output.interval);

    output.scenario = input.scenario;


    // note that this program's timezone is UTC

    time_t time_t_now = std::chrono::system_clock::to_time_t(output.timestamp);
    // 0 == vernal point (spring)
    float year_angle = (gmtime(&time_t_now)->tm_yday - 79) / 365.f * 2.f * M_PIf;

    output.sun_light_dir = -vec3(cosf(year_angle), 0.f, sinf(year_angle));

    // 2015-04-18 18:56:56
    auto new_moon = std::chrono::system_clock::from_time_t(1429379816);
    float moon_phase_time = time_interval(output.timestamp - new_moon);

    float moon_phase_angle = fmodf(moon_phase_time / 2551442.9f, 1.f) * 2.f * M_PIf;

    // FIXME: Perigee, apogee and vertical angle (against ecliptic)
    output.moon_pos = 383397.79163f * vec3(cosf(year_angle + moon_phase_angle),
                                           0.f,
                                           sinf(year_angle + moon_phase_angle));

    output.moon_angle_to_sun = moon_phase_angle;

    tm last_midnight_tm = *gmtime(&time_t_now);
    last_midnight_tm.tm_hour = last_midnight_tm.tm_min = last_midnight_tm.tm_sec = 0;

    auto last_midnight = std::chrono::system_clock::from_time_t(mktime(&last_midnight_tm));
    float second_of_day = time_interval(output.timestamp - last_midnight);

    output.earth_angle  = second_of_day / 86164.09f * 2.f * M_PIf;
    output.earth_angle -= year_angle;

    output.earth_mv = mat4::identity().scaled(vec3(6371e3f, 6371e3f, 6371e3f))
                                      .rotated(.41f, vec3(1.f, 0.f, 0.f))
                                      .rotated(output.earth_angle, vec3(0.f, 1.f, 0.f));
    output.earth_inv_mv = output.earth_mv.inverse();


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
    execute_flight_control_software(player, user_input, output.interval);


    for (int i = 0; i < static_cast<int>(input.ships.size()); i++) {
        const ShipState &in = input.ships[i];
        ShipState &out = output.ships[i];

        // Positive Z is backwards
        mat3 local_mat(in.right, in.up, -in.forward);


        bool physics_enabled = player_physics_enabled || i != input.player_ship;

        if (physics_enabled) {
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

            if (in.position.length() > 6371e3f) {
                forces += -in.position *
                          6.67384e-11 * in.total_mass * 5.974e24
                          / pow(in.position.length(), 3.);
            }

            forces += in.weapon_force;
            torque += in.weapon_torque;

            out.acceleration = forces / in.total_mass;
            out.torque       = torque;

            out.velocity = in.velocity + in.acceleration * output.interval;
            out.position = in.position + in.velocity * output.interval;
        } else {
            if (player_fixed_to_ground) {
                vec3 tangent = vec3(input.earth_mv * vec4(0.f, 1.f, 0.f, 0.f)).cross(in.position).normalized();
                vec3 sphere_pos = (input.earth_inv_mv * vec4::direction(in.position)).normalized();
                sphere_pos.y() = 0.f;
                vec3 earth_velocity = sphere_pos.length() * 6371e3f * 2.f * M_PIf / 86164.09f * tangent;

                out.velocity = earth_velocity;

                if (!fixed_to_ground_length) {
                    fixed_to_ground_length = in.position.length();
                }
                out.position = vec3(output.earth_mv * (input.earth_inv_mv * vec4::direction(in.position)));
                out.position = out.position.normalized() * fixed_to_ground_length;
            } else {
                out.velocity = vec3::zero();
                out.position = in.position;
            }

            out.acceleration = vec3::zero();
            out.torque = vec3::zero();
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
            vec3 earth_normal = out.position.normalized();
            out.velocity = .8f * (out.velocity - 2.f * out.velocity.dot(earth_normal) * earth_normal);
            out.position = 6371e3f / out.position.length() * out.position;
        }

        if (physics_enabled) {
            out.angular_momentum    = in.angular_momentum + out.torque * output.interval;
            out.rotational_velocity = out.angular_momentum / in.total_mass;
        } else {
            out.angular_momentum    = vec3::zero();
            out.rotational_velocity = vec3::zero();
        }

        if (physics_enabled) {
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

        // .transpose() == .invert() (local_mat is a rotation matrix)
        local_mat.transpose();
        out.local_velocity            = local_mat * out.velocity;
        out.local_acceleration        = local_mat * out.acceleration;
        out.local_rotational_velocity = local_mat * out.rotational_velocity;


        handle_weapons(output, input, user_input, out, in,
                       &out.weapon_force, &out.weapon_torque);
    }


    handle_particles(output.particles, input.particles, output, player);


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

    scenario = get_scenario(sn);
    if (!scenario) {
        throw std::runtime_error("Unknown scenario " + sn + " specified");
    }
    scenario_initialized = false;

    ships.emplace_back(ship_types["mumeifune"]);
    player_ship = 0;

    if (global_options.aurora) {
        auroras.resize(3);
    }
}


ShipState &WorldState::spawn_ship(const Ship *type)
{
    ships.emplace_back(type);
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
