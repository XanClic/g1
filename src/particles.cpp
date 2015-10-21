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


void spawn_particle(WorldState &output, const vec<3, double> &position,
                    const vec3 &velocity, const vec3 &orientation)
{
    output.new_particles.emplace_back();

    Particle &np = output.new_particles.back();
    np.position = position;
    np.velocity = velocity;
    np.orientation = orientation;
    np.lifetime = 120.f;
}


void handle_particles(Particles &output, const Particles &input,
                      WorldState &out_ws, const ShipState &player)
{
    size_t out_i = 0;

    vec<3, double> cam_pos = player.position + mat3(player.right, player.up, player.forward) * player.ship->cockpit_position;

    for (const Particle &p: input) {
        vec3 movement = p.velocity * out_ws.interval;
        float new_lifetime = p.lifetime - out_ws.interval;

        if (new_lifetime <= 0.f) {
            continue;
        }

        if (output.size() <= out_i) {
            output.emplace_back();
        }

        vec3 gravitation = -p.position *
                           6.67384e-11 * 5.974e24
                           / pow(p.position.length(), 3.);

        Particle &op = output[out_i++];
        op.position = p.position + movement;
        op.velocity = p.velocity + gravitation * out_ws.interval;
        op.orientation = p.orientation;
        op.lifetime = new_lifetime;

        op.position_relative_to_viewer = vec3(op.position - cam_pos);
    }

    for (const Particle &p: out_ws.new_particles) {
        if (output.size() <= out_i) {
            output.emplace_back();
        }

        Particle &op = output[out_i++];
        op.position = p.position;
        op.velocity = p.velocity;
        op.orientation = p.orientation;
        op.lifetime = p.lifetime;

        op.position_relative_to_viewer = vec3(op.position - cam_pos);
    }

    out_ws.new_particles.clear();

    if (out_i < output.size()) {
        output.resize(out_i);
    }
}


void draw_particles(const GraphicsStatus &status, const Particles &input)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    particle_data->set_elements(input.size());

    particle_data->attrib(0)->data(input.data(),
                                   input.size() * sizeof(Particle),
                                   GL_DYNAMIC_DRAW, false);

    particle_data->attrib(0)->load(sizeof(Particle),
                                   offsetof(Particle,
                                            position_relative_to_viewer));
    particle_data->attrib(1)->load(sizeof(Particle),
                                   offsetof(Particle, orientation));

    particle_prg->use();

    particle_prg->uniform<mat4>("mat_mvp") = status.projection
                                             * status.relative_to_camera;
    particle_prg->uniform<float>("aspect") = status.aspect;

    particle_data->draw(GL_POINTS);

    glEnable(GL_CULL_FACE);
}
