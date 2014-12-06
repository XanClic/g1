#include <dake/dake.hpp>

#include <cstdlib>
#include <cstring>

#include "cockpit.hpp"
#include "localize.hpp"
#include "options.hpp"
#include "text.hpp"


// #define COCKPIT_SUPERSAMPLING


using namespace dake;
using namespace dake::math;


static gl::vertex_array *line_va;
static gl::program *line_prg, *scratch_prg;
static gl::texture *scratch_tex, *normals_tex;
static gl::framebuffer *cockpit_fb;
#ifdef COCKPIT_SUPERSAMPLING
static gl::framebuffer *cockpit_ms_fb;
#endif

static unsigned width, height;


static void resize(unsigned w, unsigned h);


void init_cockpit(void)
{
    line_va = new gl::vertex_array;
    line_va->set_elements(2);

    float boole[] = {0.f, 1.f};

    line_va->attrib(0)->format(1);
    line_va->attrib(0)->data(boole);


    line_prg = new gl::program {gl::shader::vert("shaders/line_vert.glsl"),
                                gl::shader::frag("shaders/line_frag.glsl")};

    line_prg->bind_attrib("va_segment", 0);
    line_prg->bind_frag("out_col", 0);

    if (global_options.uniform_scratch_map) {
        scratch_prg = new gl::program {gl::shader::vert("shaders/scratch_vert.glsl"),
                                       gl::shader::frag("shaders/scratch_uniform_frag.glsl")};
    } else {
        scratch_prg = new gl::program {gl::shader::vert("shaders/scratch_vert.glsl"),
                                       gl::shader::frag("shaders/scratch_frag.glsl")};
    }

    scratch_prg->bind_attrib("va_pos", 0);
    scratch_prg->bind_frag("out_col", 0);


    scratch_tex = new gl::texture(std::string("assets/scratches-") +
                                  (global_options.uniform_scratch_map ? "uniform-" : "") +
                                  std::to_string(global_options.scratch_map_resolution) +
                                  "p.png");
    scratch_tex->set_tmu(1);
    scratch_tex->filter(GL_LINEAR);

    normals_tex = new gl::texture("assets/cockpit_normals.png");
    normals_tex->set_tmu(2);
    normals_tex->filter(GL_LINEAR);
    normals_tex->wrap(GL_CLAMP);

    cockpit_fb    = new gl::framebuffer(1, GL_R11F_G11F_B10F);
#ifdef COCKPIT_SUPERSAMPLING
    cockpit_ms_fb = new gl::framebuffer(1, GL_R11F_G11F_B10F, gl::framebuffer::DEPTH_ONLY, 4);
#endif

    register_resize_handler(resize);
}


static void resize(unsigned w, unsigned h)
{
    cockpit_fb->resize(w, h);
#ifdef COCKPIT_SUPERSAMPLING
    cockpit_ms_fb->resize(w, h);
#endif

    width = w;
    height = h;
}


static void draw_line(const vec2 &start, const vec2 &end)
{
    line_prg->use();

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
    return project(status, vec4::direction(vector));
}


static float smoothstep(float edge0, float edge1, float x)
{
    x = helper::maximum(0.f, helper::minimum(1.f, (x - edge0) / (edge1 - edge0)));
    return x * x * (3.f - 2.f * x);
}


static bool in_bounds(const vec2 &proj, const vec2 &bx, const vec2 &by)
{
    return (proj.x() >= bx[0]) && (proj.x() <= bx[1]) &&
           (proj.y() >= by[0]) && (proj.y() <= by[1]);
}


static vec2 project_clamp_to_border(const GraphicsStatus &status, const vec4 &vector, const vec2 &bx, const vec2 &by,
                                    float sxs, float sys, bool *visible = nullptr)
{
    float dp = status.camera_forward.dot(vec3(vector));

    vec2 proj = project(status, vector);

    if ((dp > 0.f) && in_bounds(proj, bx, by)) {
        if (visible) {
            *visible = true;
        }
    } else {
        if (visible) {
            *visible = false;
        }

        if (dp > 0.f) {
            proj =  proj * helper::minimum((fabsf(bx[proj.x() > 0.f]) - 1.5f * sxs) / fabsf(proj.x()),
                                           (fabsf(by[proj.y() > 0.f]) - 1.5f * sys) / fabsf(proj.y()));
        } else {
            proj = -proj * helper::minimum((fabsf(bx[proj.x() < 0.f]) - 1.5f * sxs) / fabsf(proj.x()),
                                           (fabsf(by[proj.y() < 0.f]) - 1.5f * sys) / fabsf(proj.y()));
        }
    }

    return proj;
}


static vec2 project_clamp_to_border(const GraphicsStatus &status, const vec3 &vector, const vec2 &bx, const vec2 &by,
                                    float sxs, float sys, bool *visible = nullptr)
{
    return project_clamp_to_border(status, vec4::direction(vector), bx, by, sxs, sys, visible);
}


void draw_cockpit(const GraphicsStatus &status, const WorldState &world)
{
    // FIXME
    static float cockpit_brightness = 1.f;
    cockpit_brightness += .4f * world.interval * (status.luminance - cockpit_brightness);

    if (cockpit_brightness < .2f) {
        cockpit_brightness = .2f;
    }

    const ShipState &ship = world.ships[world.player_ship];


    gl::framebuffer *main_fb = gl::framebuffer::current();

    cockpit_fb->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    (*main_fb)[0].bind();
    scratch_tex->bind();
    normals_tex->bind();

    glDepthMask(GL_FALSE);

    float earth_sun_angle = acosf(ship.position.normalized().dot(world.sun_light_dir));
    float earth_dim_angle = asinf(6371.f / ship.position.length());

    float sun_strength       = smoothstep(.94f, 1.07f, earth_sun_angle / earth_dim_angle);
    float sun_bloom_strength = smoothstep(.93f, 1.03f, earth_sun_angle / earth_dim_angle);

    // TODO: Could reuse from draw_environment()
    vec4 sun_pos = 149.6e6f * -world.sun_light_dir;
    sun_pos.w() = 1.f;
    sun_pos = status.world_to_camera * sun_pos;

    vec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    scratch_prg->use();
    scratch_prg->uniform<vec3>("sun_dir")       = world.sun_light_dir * sun_strength;
    scratch_prg->uniform<vec3>("sun_bloom_dir") = world.sun_light_dir * sun_bloom_strength;
    if (!global_options.uniform_scratch_map) {
        scratch_prg->uniform<vec2>("sun_position")  = projected_sun;
    }
    scratch_prg->uniform<vec3>("forward") = ship.forward;
    scratch_prg->uniform<vec3>("right")   = ship.right;
    scratch_prg->uniform<vec3>("up")      = ship.up;
    scratch_prg->uniform<mat3>("normal_mat") = mat3(ship.right, ship.up, ship.forward);
    scratch_prg->uniform<float>("aspect") = status.aspect / (16.f / 9.f);
    scratch_prg->uniform<float>("xhfov") = status.yfov * status.width / status.height / 2.f;
    scratch_prg->uniform<float>("yhfov") = status.yfov / 2.f;
    scratch_prg->uniform<gl::texture>("fb") = (*main_fb)[0];
    scratch_prg->uniform<gl::texture>("scratches") = *scratch_tex;
    scratch_prg->uniform<gl::texture>("normals") = *normals_tex;

    quad_vertices->draw(GL_TRIANGLE_STRIP);

    glDepthMask(GL_TRUE);


#ifdef COCKPIT_SUPERSAMPLING
    cockpit_ms_fb->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    cockpit_fb->blit();
#endif


    float aspect = static_cast<float>(status.width) / status.height;
    static float blink_time = 0.f;
    float sxs = .01f, sys = .01f * aspect;

    blink_time = fmodf(blink_time + world.interval, 1.f);

    set_text_color(vec4(0.f, cockpit_brightness, 0.f, 1.f));

    const vec3 &velocity = ship.velocity;


    draw_text(vec2(-1.f + .5f * sxs, 1.f - 1.5f * sys), vec2(sxs, 2 * sys), localize(LS_ORBITAL_VELOCITY));
    draw_text(vec2(-1.f + .5f * sxs, 1.f - 3.5f * sys), vec2(sxs, 2 * sys), localize(velocity.length()));

    draw_text(vec2(-1.f + .5f * sxs, 1.f - 5.5f * sys), vec2(sxs, 2 * sys), localize(LS_HEIGHT_OVER_GROUND));
    draw_text(vec2(-1.f + .5f * sxs, 1.f - 7.5f * sys), vec2(sxs, 2 * sys), localize(ship.position.length() - 6371.f));


    unsigned hud_height = height * 3 / 4;

    glEnable(GL_SCISSOR_TEST);
    glScissor((width - hud_height) / 2, height - hud_height, hud_height, hud_height);

    vec2 hbx, hby;
    hbx[0] = -.75f * status.height / status.width;
    hbx[1] = -hbx[0];
    hby[0] = -.5f;
    hby[1] = 1.f;


    line_prg->uniform<vec4>("color") = vec4(0.f, cockpit_brightness, 0.f, 1.f);

    vec2 fwd_proj = project(status, ship.forward);
    draw_line(fwd_proj + vec2(-sxs, 0.f), fwd_proj + vec2(sxs, 0.f));
    draw_line(fwd_proj + vec2(0.f, -sys), fwd_proj + vec2(0.f, sys));


    if (velocity.length()) {
        bool fwd_visible, bwd_visible;
        vec2 proj_fwd_vlcty = project_clamp_to_border(status,  velocity, hbx, hby, sxs, sys, &fwd_visible);
        vec2 proj_bwd_vlcty = project_clamp_to_border(status, -velocity, hbx, hby, sxs, sys, &bwd_visible);

        if (fwd_visible || !bwd_visible) {
            line_prg->uniform<vec4>("color") = vec4(0.f, cockpit_brightness, 0.f, fwd_visible ? 1.f : .3f);
            draw_line(proj_fwd_vlcty + vec2(-sxs, -sys), proj_fwd_vlcty + vec2( sxs,  sys));
            draw_line(proj_fwd_vlcty + vec2( sxs, -sys), proj_fwd_vlcty + vec2(-sxs,  sys));
        }


        if (bwd_visible || !fwd_visible) {
            line_prg->uniform<vec4>("color") = vec4(0.f, cockpit_brightness, 0.f, bwd_visible ? 1.f : .3f);
            draw_line(proj_bwd_vlcty + vec2(-sxs,  sys), proj_bwd_vlcty + vec2(-sxs, -sys));
            draw_line(proj_bwd_vlcty + vec2(-sxs, -sys), proj_bwd_vlcty + vec2( sxs, -sys));
            draw_line(proj_bwd_vlcty + vec2( sxs, -sys), proj_bwd_vlcty + vec2( sxs,  sys));
            draw_line(proj_bwd_vlcty + vec2( sxs,  sys), proj_bwd_vlcty + vec2(-sxs,  sys));
        }


        vec3 orbit_forward = velocity.normalized();
        vec3 orbit_inward  = -ship.position.normalized();
        vec3 orbit_upward  = orbit_forward.cross(orbit_inward).normalized();

        line_prg->uniform<vec4>("color") = vec4(0.f, cockpit_brightness, 0.f, 1.f);

        vec3 zero = (status.camera_forward - status.camera_forward.dot(orbit_upward) * orbit_upward).normalized();
        vec3 rvec = zero.cross(orbit_upward);
        for (int angle = -90; angle <= 90; angle += 2) {
            float ra = M_PIf * angle / 180.f;

            vec3 vec = mat3(mat4::identity().rotated(ra, rvec)) * zero;
            vec2 proj_vec = project(status, vec);

            if ((status.camera_forward.dot(vec) > 0.f) && in_bounds(proj_vec, hbx, hby)) {
                vec2 dvec = project(status, mat3(mat4::identity().rotated(ra + .01f, rvec)) * zero);

                dvec = ((angle % 10) ? .5f : 2.f) * vec2(-dvec.y(), dvec.x() * aspect).normalized();
                vec2 pos[2] = {
                    proj_vec - vec2(dvec.x() * sxs, dvec.y() * sys),
                    proj_vec + vec2(dvec.x() * sxs, dvec.y() * sys)
                };

                draw_line(pos[0], pos[1]);

                if (!(angle % 10)) {
                    draw_text((pos[0].x() > pos[1].x() ? pos[0] : pos[1]) + vec2(sxs, 0.f), vec2(sxs, 2 * sys),
                              localize(angle));
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

        if ((status.camera_forward.dot(vec) > 0.f) && in_bounds(proj_vec, hbx, hby)) {
            vec2 dvec = project(status, mat3(mat4::identity().rotated(ra + .01f, rvec)) * horizont);

            dvec = ((angle % 10) ? 1.5f : 10.f) * vec2(-dvec.y(), dvec.x() * aspect).normalized();
            vec2 pos[2] = {
                proj_vec - vec2(dvec.x() * sxs, dvec.y() * sys),
                proj_vec + vec2(dvec.x() * sxs, dvec.y() * sys)
            };

            draw_line(pos[0], pos[1]);

            if (!(angle % 10)) {
                draw_text((pos[0].x() > pos[1].x() ? pos[0] : pos[1]) + vec2(sxs, 0.f), vec2(sxs, 2 * sys),
                          localize(angle));
            }
        }
    }


    glDisable(GL_SCISSOR_TEST);


    main_fb->bind();
#ifdef COCKPIT_SUPERSAMPLING
    cockpit_ms_fb->blit();
#else
    cockpit_fb->blit();
#endif
}
