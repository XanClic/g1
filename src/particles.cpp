#include <dake/math/matrix.hpp>
#include <dake/gl.hpp>

#include <cmath>
#include <cstdlib>

#include "graphics.hpp"
#include "particles.hpp"
#include "physics.hpp"
#include "ship.hpp"


using namespace dake;
using namespace dake::math;


static gl::vertex_array *particle_data;
static gl::program *particle_prg;


void init_particles(void)
{
    particle_prg = new gl::program {gl::shader::vert("shaders/particle_vert.glsl"),
                                    gl::shader::geom("shaders/particle_geom.glsl"),
                                    gl::shader::frag("shaders/particle_frag.glsl")};

    particle_prg->bind_attrib("va_position", 0);
    particle_prg->bind_attrib("va_orientation", 1);
    particle_prg->bind_frag("out_col", 0);

    particle_data = new gl::vertex_array;

    particle_data->attrib(0)->format(3);
    particle_data->attrib(1)->format(3);
    particle_data->attrib(1)->reuse_buffer(particle_data->attrib(0));
}


void spawn_particle(WorldState &output, const ShipState &sender,
                    const vec<3, double> &position, const vec3 &velocity,
                    const vec3 &orientation)
{
    output.new_particles.pgd.emplace_back();

    ParticleGraphicsData &pgd = output.new_particles.pgd.back();
    pgd.orientation = orientation;


    output.new_particles.pngd.emplace_back();

    ParticleNonGraphicsData &pngd = output.new_particles.pngd.back();
    pngd.position = position;
    pngd.velocity = velocity;
    pngd.lifetime = 120.f;
    pngd.source_ship_id = sender.id;
}


void handle_particles(Particles &output, const Particles &input,
                      WorldState &out_ws, const ShipState &player)
{
    size_t out_i = 0;

    vec<3, double> cam_pos = player.position +
                             mat3(player.right, player.up, player.forward)
                             * player.ship->cockpit_position;

    size_t isz = input.pgd.size(), osz = output.pgd.size();

    for (size_t i = 0; i < isz; i++) {
        const ParticleGraphicsData &pgd = input.pgd[i];
        const ParticleNonGraphicsData &pngd = input.pngd[i];

        float new_lifetime = pngd.lifetime - out_ws.interval;

        if (new_lifetime <= 0.f) {
            continue;
        }

        if (osz <= out_i) {
            output.pgd.emplace_back();
            output.pngd.emplace_back();

            osz++;
        }

        vec3 gravitation = -pngd.position *
                           6.67384e-11 * 5.974e24
                           / pow(pngd.position.length(), 3.);

        ParticleGraphicsData &opgd = output.pgd[out_i];
        ParticleNonGraphicsData &opngd = output.pngd[out_i];
        out_i++;

        opngd.velocity = pngd.velocity + gravitation * out_ws.interval;
        opngd.position = pngd.position + opngd.velocity * out_ws.interval;
        opngd.lifetime = new_lifetime;
        opngd.source_ship_id = pngd.source_ship_id;

        opgd.position_relative_to_viewer = vec3(opngd.position - cam_pos);
        opgd.orientation = pgd.orientation;
    }

    size_t nsz = out_ws.new_particles.pgd.size();

    for (size_t i = 0; i < nsz; i++) {
        const ParticleGraphicsData &pgd = out_ws.new_particles.pgd[i];
        const ParticleNonGraphicsData &pngd = out_ws.new_particles.pngd[i];

        if (osz <= out_i) {
            output.pgd.emplace_back();
            output.pngd.emplace_back();

            osz++;
        }

        ParticleGraphicsData &opgd = output.pgd[out_i];
        ParticleNonGraphicsData &opngd = output.pngd[out_i];
        out_i++;

        opngd.velocity = pngd.velocity;
        opngd.position = pngd.position;
        opngd.lifetime = pngd.lifetime;
        opngd.source_ship_id = pngd.source_ship_id;

        opgd.position_relative_to_viewer = vec3(opngd.position - cam_pos);
        opgd.orientation = pgd.orientation;
    }

    out_ws.new_particles.pgd.clear();
    out_ws.new_particles.pngd.clear();

    if (out_i < osz) {
        output.pgd.resize(out_i);
        output.pngd.resize(out_i);
    }


#define HITBOX_RADIUS 1.f

    // TODO: Put particle and ship positions into a kd tree to reduce collision
    // testing complexity
    for (size_t i = 0; i < out_i; i++) {
        ParticleNonGraphicsData &pngd = output.pngd[i];

        for (ShipState &s: out_ws.ships) {
            vec3 movement = -out_ws.interval * pngd.velocity;
            float mvsq = movement.dot(movement);
            float dist_end = (pngd.position - s.position).length();

            if (dist_end < HITBOX_RADIUS + sqrtf(mvsq) &&
                pngd.source_ship_id != s.id)
            {
                // Find nearest point
                float t = (s.position - pngd.position).dot(movement) / mvsq;
                float min_dist;

                min_dist = (pngd.position + t * movement - s.position).length();

                if (min_dist <= HITBOX_RADIUS) {
                    float dist_start =
                        (pngd.position + movement - s.position).length();

                    if ((t >= 0.f && t <= 1.f) ||
                        dist_end <= HITBOX_RADIUS ||
                        dist_start <= HITBOX_RADIUS)
                    {
                        pngd.lifetime = 0.f;
                        s.deal_damage(10.f);
                    }
                }
            }
        }
    }
}


void draw_particles(const GraphicsStatus &status, const Particles &input)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    particle_data->set_elements(input.pgd.size());

    particle_data->attrib(0)->data(input.pgd.data(),
                                   input.pgd.size()
                                   * sizeof(ParticleGraphicsData),
                                   GL_DYNAMIC_DRAW, false);

    particle_data->attrib(0)->load(sizeof(ParticleGraphicsData),
                                   offsetof(ParticleGraphicsData,
                                            position_relative_to_viewer));
    particle_data->attrib(1)->load(sizeof(ParticleGraphicsData),
                                   offsetof(ParticleGraphicsData, orientation));

    particle_prg->use();

    particle_prg->uniform<mat4>("mat_mvp") = status.projection
                                             * status.relative_to_camera;
    particle_prg->uniform<float>("aspect") = status.aspect;

    particle_data->draw(GL_POINTS);

    glEnable(GL_CULL_FACE);
}
