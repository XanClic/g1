#include <dake/dake.hpp>

#include <cstdlib>
#include <cstring>

#include "cockpit.hpp"
#include "localize.hpp"


using namespace dake;
using namespace dake::math;


static gl::vertex_array *line_va, *char_va;
static gl::program *line_prg, *char_prg;
static gl::texture *latin_font, *mado_font, *tengwar_font;


struct CharVAElement {
    vec2 pos;
    unsigned chr;
};


void init_cockpit(void)
{
    line_va = new gl::vertex_array;
    line_va->set_elements(2);

    float boole[] = {0.f, 1.f};

    line_va->attrib(0)->format(1);
    line_va->attrib(0)->data(boole);


    CharVAElement *cvad = new CharVAElement[320];
    for (int i = 0; i < 320; i++) {
        float ysign = (i % 10 >= 5) ? -1.f : 1.f;
        int m = i % 5;

        if (m == 4) {
            m = 3;
        }

        ysign = (m < 2) ? -ysign : ysign;

        cvad[i].pos = vec2(i / 5 + (m % 2) * .9999f, ysign * .5f);
    }

    char_va = new gl::vertex_array;
    char_va->set_elements(320);

    char_va->attrib(0)->format(2);
    char_va->attrib(0)->data(cvad, 320 * sizeof(CharVAElement), GL_STREAM_DRAW, false);
    char_va->attrib(0)->load(sizeof(CharVAElement), offsetof(CharVAElement, pos));

    char_va->attrib(1)->format(1, GL_UNSIGNED_INT);
    char_va->attrib(1)->reuse_buffer(char_va->attrib(0));


    line_prg = new gl::program {gl::shader::vert("assets/line_vert.glsl"), gl::shader::frag("assets/line_frag.glsl")};
    line_prg->bind_attrib("va_segment", 0);
    line_prg->bind_frag("out_col", 0);

    char_prg = new gl::program {gl::shader::vert("assets/char_vert.glsl"), gl::shader::frag("assets/char_frag.glsl")};
    char_prg->bind_attrib("va_pos", 0);
    char_prg->bind_attrib("va_char", 1);
    char_prg->bind_frag("out_col", 0);


    latin_font = new gl::texture("assets/latin.png");
    latin_font->filter(GL_NEAREST);

    mado_font = new gl::texture("assets/mado.png");
    mado_font->filter(GL_NEAREST);

    tengwar_font = new gl::texture("assets/tengwar.png");
    tengwar_font->filter(GL_NEAREST);
}


static void draw_line(const vec2 &start, const vec2 &end)
{
    line_prg->uniform<vec2>("start") = start;
    line_prg->uniform<vec2>("end")   = end;

    line_va->draw(GL_LINES);
}


static void draw_text(const vec2 &pos, const vec2 &size, const std::string &text)
{
    std::vector<unsigned> tt;
    for (int i = 0; text[i]; i++) {
        if (text[i] & 0x80) {
            if ((text[i] & 0xe0) == 0xc0) {
                if ((text[i + 1] & 0xc0) == 0x80) {
                    unsigned v = ((text[i] & 0x1f) << 6) | (text[i + 1] & 0x3f);
                    ++i;
                    if (v < 256) {
                        tt.push_back(v);
                    } else {
                        tt.push_back(0);
                    }
                } else {
                    tt.push_back(0);
                }
            } else {
                tt.push_back(0);
            }
        } else {
            tt.push_back(text[i]);
        }
    }

    if (tt.size() > 64) {
        throw std::invalid_argument("Text is too long");
    }

    CharVAElement *cvad = static_cast<CharVAElement *>(char_va->attrib(0)->map());
    for (int i = 0; i < static_cast<int>(tt.size()); i++) {
        for (int j = 0; j < 5; j++) {
            cvad[i * 5 + j].chr = tt[i];
        }
    }
    char_va->attrib(0)->unmap();

    char_va->attrib(1)->load(sizeof(CharVAElement), offsetof(CharVAElement, chr));

    char_prg->use();
    char_prg->uniform<vec2>("position") = pos;
    char_prg->uniform<vec2>("char_size") = size;

    char_va->set_elements(tt.size() * 5);
    char_va->draw(GL_TRIANGLE_STRIP);
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

    gl::texture *font = (olo == DE_MADO)    ? mado_font
                      : (olo == DE_TENGWAR) ? tengwar_font
                      :                       latin_font;

    font->bind();

    char_prg->uniform<vec4>("color") = vec4(0.f, 1.f, 0.f, 1.f);
    char_prg->uniform<gl::texture>("font") = *font;

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
