#include <dake/dake.hpp>

#include "cockpit.hpp"


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


void draw_cockpit(const GraphicsStatus &status, const WorldState &world)
{
    float aspect = static_cast<float>(status.width) / status.height;
    static float blink_time = 0.f;
    float sxs = .01f, sys = .01f * aspect;
    float sxb = 1.f - 3.f * sxs, syb = 1.f - 3.f * sys;

    blink_time = fmodf(blink_time + world.interval, 1.f);

    line_prg->use();

    line_prg->uniform<vec4>("color") = vec4(.5f, 1.f, .5f, 1.f);
    draw_line(vec2(-sxs, 0.f), vec2(sxs, 0.f));
    draw_line(vec2(0.f, -sys), vec2(0.f, sys));

    if (world.player_velocity.length()) {
        float fdp = world.player_forward.dot(world.player_velocity);

        vec2 proj_fwd_vlcty = project(status,  vec4(world.player_velocity.x(), world.player_velocity.y(), world.player_velocity.z(), 0.f));
        bool fwd_visible = (fdp > 0.f) && (fabsf(proj_fwd_vlcty.x()) <= sxb) && (fabsf(proj_fwd_vlcty.y()) <= syb);

        vec2 proj_bwd_vlcty = project(status, -vec4(world.player_velocity.x(), world.player_velocity.y(), world.player_velocity.z(), 0.f));
        bool bwd_visible = (fdp < 0.f) && (fabsf(proj_bwd_vlcty.x()) <= sxb) && (fabsf(proj_bwd_vlcty.y()) <= syb);

        if (fwd_visible || !bwd_visible) {
            if (!fwd_visible) {
                if (fdp > 0.f) {
                    proj_fwd_vlcty =  proj_fwd_vlcty * helper::minimum(sxb / fabsf(proj_fwd_vlcty.x()), syb / fabsf(proj_fwd_vlcty.y()));
                } else {
                    proj_fwd_vlcty = -proj_fwd_vlcty * helper::minimum(sxb / fabsf(proj_fwd_vlcty.x()), syb / fabsf(proj_fwd_vlcty.y()));
                }
            }

            line_prg->uniform<vec4>("color") = vec4(.5f, 1.f, .5f, fwd_visible ? 1.f : .3f);
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

            line_prg->uniform<vec4>("color") = vec4(.5f, 1.f, .5f, bwd_visible ? 1.f : .3f);
            draw_line(proj_bwd_vlcty + vec2(-sxs,  sys), proj_bwd_vlcty + vec2(-sxs, -sys));
            draw_line(proj_bwd_vlcty + vec2(-sxs, -sys), proj_bwd_vlcty + vec2( sxs, -sys));
            draw_line(proj_bwd_vlcty + vec2( sxs, -sys), proj_bwd_vlcty + vec2( sxs,  sys));
            draw_line(proj_bwd_vlcty + vec2( sxs,  sys), proj_bwd_vlcty + vec2(-sxs,  sys));
        }
    }
}
