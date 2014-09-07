#include <dake/dake.hpp>

#include <cstdlib>
#include <cstring>

#include "cockpit.hpp"
#include "localize.hpp"
#include "text.hpp"


using namespace dake;
using namespace dake::math;


static gl::vertex_array *line_va;
static gl::program *line_prg;


void init_cockpit(void)
{
    line_va = new gl::vertex_array;
    line_va->set_elements(2);

    float boole[] = {0.f, 1.f};

    line_va->attrib(0)->format(1);
    line_va->attrib(0)->data(boole);


    line_prg = new gl::program {gl::shader::vert("assets/line_vert.glsl"), gl::shader::frag("assets/line_frag.glsl")};
    line_prg->bind_attrib("va_segment", 0);
    line_prg->bind_frag("out_col", 0);
}


static void draw_line(const vec2 &start, const vec2 &end)
{
    line_prg->uniform<vec2>("start") = start;
    line_prg->uniform<vec2>("end")   = end;

    line_va->draw(GL_LINES);
}


static vec2 project(const GraphicsStatus &status, const vec4 &vector)
{
    vec4 proj = status.projection * status.world_to_camera * vector;

    return vec2(proj) / proj.w();
}


static vec2 project(const GraphicsStatus &status, const vec3 &vector)
{
    return project(status, vec4(vector));
}


void draw_cockpit(const GraphicsStatus &status, const WorldState &world)
{
    float aspect = static_cast<float>(status.width) / status.height;
    static float blink_time = 0.f;
    float sxs = .01f, sys = .01f * aspect;
    float sxb = 1.f - 3.f * sxs, syb = 1.f - 3.f * sys;

    blink_time = fmodf(blink_time + world.interval, 1.f);

    set_text_color(vec4(0.f, 1.f, 0.f, 1.f));

    line_prg->use();

    line_prg->uniform<vec4>("color") = vec4(0.f, 1.f, 0.f, 1.f);
    draw_line(vec2(-sxs, 0.f), vec2(sxs, 0.f));
    draw_line(vec2(0.f, -sys), vec2(0.f, sys));

    const ShipState &ship = world.ships[world.player_ship];
    const vec3 &velocity = ship.velocity;


    draw_text(vec2(-1.f + .5f * sxs, 1.f - 1.5f * sys), vec2(sxs, 2 * sys), localize(LS_ORBITAL_VELOCITY));
    draw_text(vec2(-1.f + .5f * sxs, 1.f - 3.5f * sys), vec2(sxs, 2 * sys), localize(velocity.length()));

    draw_text(vec2(-1.f + .5f * sxs, 1.f - 5.5f * sys), vec2(sxs, 2 * sys), localize(LS_HEIGHT_OVER_GROUND));
    draw_text(vec2(-1.f + .5f * sxs, 1.f - 7.5f * sys), vec2(sxs, 2 * sys), localize(ship.position.length() - 6371.f));


    if (velocity.length()) {
        float fdp = status.camera_forward.dot(velocity);

        vec2 proj_fwd_vlcty = project(status,  velocity);
        bool fwd_visible = (fdp > 0.f) && (fabsf(proj_fwd_vlcty.x()) <= sxb) && (fabsf(proj_fwd_vlcty.y()) <= syb);

        vec2 proj_bwd_vlcty = project(status, -velocity);
        bool bwd_visible = (fdp < 0.f) && (fabsf(proj_bwd_vlcty.x()) <= sxb) && (fabsf(proj_bwd_vlcty.y()) <= syb);

        if (fwd_visible || !bwd_visible) {
            if (!fwd_visible) {
                if (fdp > 0.f) {
                    proj_fwd_vlcty =  proj_fwd_vlcty * helper::minimum(sxb / fabsf(proj_fwd_vlcty.x()), syb / fabsf(proj_fwd_vlcty.y()));
                } else {
                    proj_fwd_vlcty = -proj_fwd_vlcty * helper::minimum(sxb / fabsf(proj_fwd_vlcty.x()), syb / fabsf(proj_fwd_vlcty.y()));
                }
            }

            line_prg->uniform<vec4>("color") = vec4(0.f, 1.f, 0.f, fwd_visible ? 1.f : .3f);
            draw_line(proj_fwd_vlcty + vec2(-sxs, -sys), proj_fwd_vlcty + vec2( sxs,  sys));
            draw_line(proj_fwd_vlcty + vec2( sxs, -sys), proj_fwd_vlcty + vec2(-sxs,  sys));
        }


        if (bwd_visible || !fwd_visible) {
            if (!bwd_visible) {
                if (fdp < 0.f) {
                    proj_bwd_vlcty =  proj_bwd_vlcty * helper::minimum(sxb / fabsf(proj_bwd_vlcty.x()), syb / fabsf(proj_bwd_vlcty.y()));
                } else {
                    proj_bwd_vlcty = -proj_bwd_vlcty * helper::minimum(sxb / fabsf(proj_bwd_vlcty.x()), syb / fabsf(proj_bwd_vlcty.y()));
                }
            }

            line_prg->uniform<vec4>("color") = vec4(0.f, 1.f, 0.f, bwd_visible ? 1.f : .3f);
            draw_line(proj_bwd_vlcty + vec2(-sxs,  sys), proj_bwd_vlcty + vec2(-sxs, -sys));
            draw_line(proj_bwd_vlcty + vec2(-sxs, -sys), proj_bwd_vlcty + vec2( sxs, -sys));
            draw_line(proj_bwd_vlcty + vec2( sxs, -sys), proj_bwd_vlcty + vec2( sxs,  sys));
            draw_line(proj_bwd_vlcty + vec2( sxs,  sys), proj_bwd_vlcty + vec2(-sxs,  sys));
        }


        vec3 orbit_forward = velocity.normalized();
        vec3 orbit_inward  = -ship.position.normalized();
        vec3 orbit_upward  = orbit_forward.cross(orbit_inward).normalized();

        line_prg->uniform<vec4>("color") = vec4(0.f, 1.f, 0.f, 1.f);

        vec3 zero = (status.camera_forward - status.camera_forward.dot(orbit_upward) * orbit_upward).normalized();
        vec3 rvec = zero.cross(orbit_upward);
        for (int angle = -90; angle <= 90; angle += 2) {
            float ra = M_PIf * angle / 180.f;

            vec3 vec = mat3(mat4::identity().rotated(ra, rvec)) * zero;
            vec2 proj_vec = project(status, vec);

            if ((status.camera_forward.dot(vec) > 0.f) && (fabsf(proj_vec.x()) < 1.f) && (fabsf(proj_vec.y()) < 1.f)) {
                vec2 dvec = project(status, mat3(mat4::identity().rotated(ra + .01f, rvec)) * zero);

                dvec = ((angle % 10) ? .5f : 2.f) * vec2(-dvec.y(), dvec.x() * aspect).normalized();
                vec2 pos[2] = {
                    proj_vec - vec2(dvec.x() * sxs, dvec.y() * sys),
                    proj_vec + vec2(dvec.x() * sxs, dvec.y() * sys)
                };

                draw_line(pos[0], pos[1]);

                if (!(angle % 10)) {
                    draw_text((pos[0].x() > pos[1].x() ? pos[0] : pos[1]) + vec2(sxs, 0.f), vec2(sxs, 2 * sys), localize(angle));
                }
            }
        }
    }


    vec3 earth_upward = ship.position.normalized();
    vec3 horizont = (status.camera_forward - status.camera_forward.dot(earth_upward) * earth_upward).normalized();
    vec3 rvec = horizont.cross(earth_upward);
    for (int angle = -90; angle <= 90; angle += 5) {
        float ra = M_PIf * angle / 180.f;

        vec3 vec = mat3(mat4::identity().rotated(ra, rvec)) * horizont;
        vec2 proj_vec = project(status, vec);

        if ((status.camera_forward.dot(vec) > 0.f) &&
            (fabsf(proj_vec.x()) < 1.f) && (fabsf(proj_vec.y()) < 1.f))
        {
            vec2 dvec = project(status, mat3(mat4::identity().rotated(ra + .01f, rvec)) * horizont);

            dvec = ((angle % 10) ? 1.5f : 10.f) * vec2(-dvec.y(), dvec.x() * aspect).normalized();
            vec2 pos[2] = {
                proj_vec - vec2(dvec.x() * sxs, dvec.y() * sys),
                proj_vec + vec2(dvec.x() * sxs, dvec.y() * sys)
            };

            draw_line(pos[0], pos[1]);

            if (!(angle % 10)) {
                draw_text((pos[0].x() > pos[1].x() ? pos[0] : pos[1]) + vec2(sxs, 0.f), vec2(sxs, 2 * sys), localize(angle));
            }
        }
    }
}
