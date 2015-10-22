#include <dake/dake.hpp>

#include <cstdlib>
#include <cstring>

#include "cockpit.hpp"
#include "graphics.hpp"
#include "localize.hpp"
#include "options.hpp"
#include "radar.hpp"
#include "text.hpp"
#include "weapons.hpp"


// #define COCKPIT_SUPERSAMPLING


using namespace dake;
using namespace dake::math;


static gl::vertex_array *line_va;
static gl::program *line_prg, *scratch_prg, *sprite_prg;
static gl::texture *scratch_tex, *normals_tex;

static gl::texture *prograde_sprite, *retrograde_sprite;
static gl::texture *radar_contact_sprite, *radar_target_sprite;
static gl::texture *target_aim_sprite;
static gl::texture *delta_positive_sprite, *delta_negative_sprite;
static gl::texture *orbit_normal_sprite, *orbit_antinormal_sprite;

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

    sprite_prg = new gl::program {gl::shader::vert("shaders/sprite_vert.glsl"),
                                  gl::shader::frag("shaders/sprite_frag.glsl")};

    sprite_prg->bind_attrib("va_pos", 0);
    sprite_prg->bind_frag("out_col", 0);


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


    prograde_sprite = new gl::texture("assets/hud/prograde.png");
    prograde_sprite->filter(GL_LINEAR);

    retrograde_sprite = new gl::texture("assets/hud/retrograde.png");
    retrograde_sprite->filter(GL_LINEAR);

    radar_contact_sprite = new gl::texture("assets/hud/radar-contact.png");
    radar_contact_sprite->filter(GL_LINEAR);

    radar_target_sprite = new gl::texture("assets/hud/radar-target.png");
    radar_target_sprite->filter(GL_LINEAR);

    target_aim_sprite = new gl::texture("assets/hud/target-aim.png");
    target_aim_sprite->filter(GL_LINEAR);

    delta_positive_sprite = new gl::texture("assets/hud/delta-positive.png");
    delta_positive_sprite->filter(GL_LINEAR);

    delta_negative_sprite = new gl::texture("assets/hud/delta-negative.png");
    delta_negative_sprite->filter(GL_LINEAR);

    orbit_normal_sprite = new gl::texture("assets/hud/normal.png");
    orbit_normal_sprite->filter(GL_LINEAR);

    orbit_antinormal_sprite = new gl::texture("assets/hud/antinormal.png");
    orbit_antinormal_sprite->filter(GL_LINEAR);


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


static void draw_sprite(const vec2 &position, const vec2 &size,
                        const gl::texture &sprite, const vec4 &color)
{
    sprite_prg->use();

    // TODO: Bindless support
    sprite.bind();

    sprite_prg->uniform<vec2>("center") = position;
    sprite_prg->uniform<vec2>("size") = size;
    sprite_prg->uniform<gl::texture>("sprite") = sprite;
    sprite_prg->uniform<vec4>("color") = color;

    quad_vertices->draw(GL_TRIANGLE_STRIP);
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


static vec2 project_clamp_to_border(const GraphicsStatus &status,
                                    const vec3 &vector,
                                    const vec2 &bx, const vec2 &by,
                                    const vec2 &size, bool *visible = nullptr)
{
    return project_clamp_to_border(status, vector, bx, by, size.x(), size.y(),
                                   visible);
}


static void draw_scratches(const GraphicsStatus &status,
                           const WorldState &world, gl::framebuffer *main_fb)
{
    const ShipState &ship = world.ships[world.player_ship];

    (*main_fb)[0].bind();
    scratch_tex->bind();
    normals_tex->bind();

    glDepthMask(GL_FALSE);

    float earth_sun_angle = acosf(status.camera_position.normalized()
                                  .dot(world.sun_light_dir));
    float earth_dim_angle = asinf(6371e3f / status.camera_position.length());

    float sun_angle_ratio = earth_sun_angle / earth_dim_angle;
    float sun_strength       = smoothstep(.94f, 1.07f, sun_angle_ratio);
    float sun_bloom_strength = smoothstep(.93f, 1.03f, sun_angle_ratio);

    // TODO: Could reuse from draw_environment()
    vec4 sun_pos = 149.6e9f * -world.sun_light_dir;
    sun_pos.w() = 1.f;
    sun_pos = status.world_to_camera * sun_pos;

    vec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    scratch_prg->use();

    scratch_prg->uniform<vec3>("sun_dir")
        = world.sun_light_dir * sun_strength;
    scratch_prg->uniform<vec3>("sun_bloom_dir")
        = world.sun_light_dir * sun_bloom_strength;

    if (!global_options.uniform_scratch_map) {
        scratch_prg->uniform<vec2>("sun_position")  = projected_sun;
    }
    scratch_prg->uniform<vec3>("cam_fwd") = ship.forward;
    scratch_prg->uniform<vec3>("cam_right") = ship.right;
    scratch_prg->uniform<vec3>("cam_up") = ship.up;
    scratch_prg->uniform<mat3>("normal_mat") = mat3(ship.right, ship.up,
                                                    ship.forward);
    scratch_prg->uniform<float>("aspect") = status.aspect / (16.f / 9.f);
    scratch_prg->uniform<float>("xhfov") = status.yfov / 2.f
                                           * status.width / status.height;
    scratch_prg->uniform<float>("yhfov") = status.yfov / 2.f;
    scratch_prg->uniform<gl::texture>("fb") = (*main_fb)[0];
    scratch_prg->uniform<gl::texture>("scratches") = *scratch_tex;
    scratch_prg->uniform<gl::texture>("normals") = *normals_tex;

    quad_vertices->draw(GL_TRIANGLE_STRIP);

    glDepthMask(GL_TRUE);
}


static void draw_cockpit_controls(const WorldState &world,
                                  float sxs, float sys)
{
    const ShipState &ship = world.ships[world.player_ship];
    const vec3 &velocity = ship.velocity;

    draw_text(vec2(-1.f + .5f * sxs, 1.f - 1.5f * sys), vec2(sxs, 2 * sys),
              localize(LS_ORBITAL_VELOCITY));
    draw_text(vec2(-1.f + .5f * sxs, 1.f - 3.5f * sys), vec2(sxs, 2 * sys),
              localize(velocity.length(), 2, LS_UNIT_M_S));

    draw_text(vec2(-1.f + .5f * sxs, 1.f - 5.5f * sys), vec2(sxs, 2 * sys),
              localize(LS_HEIGHT_OVER_GROUND));
    draw_text(vec2(-1.f + .5f * sxs, 1.f - 7.5f * sys), vec2(sxs, 2 * sys),
              localize((static_cast<float>(ship.position.length()) - 6371e3f)
                       * 1e-3f, 2, LS_UNIT_KM));

    float time_factor = world.interval / world.real_interval;
    if (time_factor > 1.f) {
        draw_text(vec2(-1.f + .5f * sxs, 1.f -  9.5f * sys), vec2(sxs, 2 * sys),
                  localize(LS_SPEED_UP));
        draw_text(vec2(-1.f + .5f * sxs, 1.f - 11.5f * sys), vec2(sxs, 2 * sys),
                  localize(time_factor, 0, LS_UNIT_TIMES));
    }
}


static void draw_target_cross(const GraphicsStatus &status,
                              const WorldState &world,
                              float sxs, float sys)
{
    const ShipState &ship = world.ships[world.player_ship];

    vec2 fwd_proj = project(status, ship.forward);
    draw_line(fwd_proj + vec2(-sxs, 0.f), fwd_proj + vec2(sxs, 0.f));
    draw_line(fwd_proj + vec2(0.f, -sys), fwd_proj + vec2(0.f, sys));
}


static void draw_velocity_indicators(const GraphicsStatus &status,
                                     const WorldState &world,
                                     float cockpit_brightness,
                                     float sxs, float sys,
                                     const vec2 &hbx, const vec2 &hby)
{
    const ShipState &ship = world.ships[world.player_ship];
    const vec3 &velocity = ship.velocity;

    if (!velocity.length()) {
        return;
    }


    vec2 indicator_size = 2.f * vec2(sxs, sys);


    bool fwd_visible, bwd_visible;
    vec2 proj_fwd_vlcty = project_clamp_to_border(status, velocity, hbx, hby,
                                                  indicator_size, &fwd_visible);
    vec2 proj_bwd_vlcty = project_clamp_to_border(status, -velocity, hbx, hby,
                                                  indicator_size, &bwd_visible);

    if (fwd_visible || !bwd_visible) {
        draw_sprite(proj_fwd_vlcty, indicator_size, *prograde_sprite,
                    vec4(0.f, cockpit_brightness, 0.f,
                         fwd_visible ? 1.f : .3f));
    }


    if (bwd_visible || !fwd_visible) {
        draw_sprite(proj_bwd_vlcty, indicator_size, *retrograde_sprite,
                    vec4(0.f, cockpit_brightness, 0.f,
                         bwd_visible ? 1.f : .3f));
    }
}


static void draw_orbit_grid(const GraphicsStatus &status,
                            const WorldState &world,
                            float cockpit_brightness,
                            float aspect, float sxs, float sys,
                            const vec2 &hbx, const vec2 &hby)
{
    const ShipState &ship = world.ships[world.player_ship];
    const vec3 &velocity = ship.velocity;

    if (!velocity.length()) {
        return;
    }

    float cfdon = status.camera_forward.dot(ship.orbit_normal);

    vec3 zero = status.camera_forward - cfdon * ship.orbit_normal;
    if (zero.length() < 1e-3f) {
        vec2 size;
        size.y() = (M_PIf / 45.f) / status.yfov;
        size.x() = size.y() / status.aspect;

        if (cfdon > 0.f) {
            draw_sprite(project(status, ship.orbit_normal), size,
                        *orbit_normal_sprite,
                        vec4(0.f, cockpit_brightness, 0.f, 1.f));
        } else {
            draw_sprite(project(status, -ship.orbit_normal), size,
                        *orbit_antinormal_sprite,
                        vec4(0.f, cockpit_brightness, 0.f, 1.f));
        }
        return;
    }
    zero.normalize();

    vec3 rvec = zero.cross(ship.orbit_normal);
    for (int angle = -90; angle <= 90; angle += 2) {
        float ra = M_PIf * angle / 180.f;

        vec3 vec = mat3(mat4::identity().rotated(ra, rvec)) * zero;
        vec2 proj_vec = project(status, vec);

        if (status.camera_forward.dot(vec) > 0.f &&
            in_bounds(proj_vec, hbx, hby))
        {
            vec2 dvec = project(status,
                                mat3(mat4::identity().rotated(ra + .01f, rvec))
                                * zero);

            dvec = ((angle % 10) ? .5f : 2.f) *
                   vec2(-dvec.y(), dvec.x() * aspect).normalized();
            vec2 pos[2] = {
                proj_vec - vec2(dvec.x() * sxs, dvec.y() * sys),
                proj_vec + vec2(dvec.x() * sxs, dvec.y() * sys)
            };

            draw_line(pos[0], pos[1]);

            if (!(angle % 10)) {
                draw_text((pos[0].x() > pos[1].x() ? pos[0] : pos[1])
                          + vec2(sxs, 0.f),
                          vec2(sxs, 2 * sys), localize(angle));
            }
        }
    }
}


static void draw_artificial_horizon(const GraphicsStatus &status,
                                    const WorldState &world,
                                    float aspect, float sxs, float sys,
                                    const vec2 &hbx, const vec2 &hby)
{
    const ShipState &ship = world.ships[world.player_ship];

    vec3 earth_upward = ship.position.normalized();
    vec3 horizont = (status.camera_forward -
                     status.camera_forward.dot(earth_upward) * earth_upward)
                    .normalized();
    vec3 rvec = horizont.cross(earth_upward);

    for (int angle = -90; angle <= 90; angle += 5) {
        float ra = M_PIf * angle / 180.f;

        vec3 vec = mat3(mat4::identity().rotated(ra, rvec)) * horizont;
        vec2 proj_vec = project(status, vec);

        if (status.camera_forward.dot(vec) > 0.f &&
            in_bounds(proj_vec, hbx, hby))
        {
            vec2 dvec = project(status,
                                mat3(mat4::identity().rotated(ra + .01f, rvec))
                                * horizont);

            dvec = ((angle % 10) ? 1.5f : 10.f) *
                   vec2(-dvec.y(), dvec.x() * aspect).normalized();
            vec2 pos[2] = {
                proj_vec - vec2(dvec.x() * sxs, dvec.y() * sys),
                proj_vec + vec2(dvec.x() * sxs, dvec.y() * sys)
            };

            draw_line(pos[0], pos[1]);

            if (!(angle % 10)) {
                draw_text((pos[0].x() > pos[1].x() ? pos[0] : pos[1])
                          + vec2(sxs, 0.f),
                          vec2(sxs, 2 * sys), localize(angle));
            }
        }
    }
}


static void draw_radar_contacts(const GraphicsStatus &status,
                                const WorldState &world,
                                float cockpit_brightness, float blink_time,
                                float sxs, float sys,
                                const vec2 &hbx, const vec2 &hby)
{
    vec2 sprite_size = 2.f * vec2(sxs, sys);

    const ShipState &player = world.ships[world.player_ship];
    const Radar &r = player.radar;

    for (const RadarTarget &t: r.targets) {
        bool visible;
        vec2 proj = project_clamp_to_border(status, t.relative_position,
                                            hbx, hby, sprite_size, &visible);

        float distance = t.relative_position.length();

        // Display the marker if one of the following is true:
        // (1) It is in view
        // (2) It is out of view, but far away (that is, it will be displayed
        //                                      constantly then)
        // (3) Otherwise (it is out of view, but close to us) it should blink
        if (visible || distance > 50e3f || blink_time < .5f) {
            vec4 color(0.f, cockpit_brightness, 0.f, visible ? 1.f : .3f);

            if (t.id == r.selected_id) {
                draw_sprite(proj, sprite_size, *radar_target_sprite, color);
            } else {
                draw_sprite(proj, sprite_size, *radar_contact_sprite, color);
            }
        }

        if (visible) {
            float rel_speed = t.relative_position.normalized()
                              .dot(t.relative_velocity);

            draw_text(proj + vec2(0.f, 3.f * sys), vec2(sxs * .5f, sys),
                      localize(distance * 1e-3f, 2, LS_UNIT_KM),
                      ALIGN_CENTER, ALIGN_BOTTOM);
            draw_text(proj + vec2(0.f, 2.f * sys), vec2(sxs * .5f, sys),
                      localize(rel_speed, 2, LS_UNIT_M_S),
                      ALIGN_CENTER, ALIGN_BOTTOM);
        }

        if (t.id == r.selected_id) {
            bool dvelp_visible, dveln_visible, aim_visible;
            vec2 dvelp_proj, dveln_proj, aim_proj;

            // Negate, since t.relative_velocity is the velocity of the target
            // relative to us; but we want it the other way around
            dvelp_proj = project_clamp_to_border(status, -t.relative_velocity,
                                                 hbx, hby, sprite_size,
                                                 &dvelp_visible);
            dveln_proj = project_clamp_to_border(status,  t.relative_velocity,
                                                 hbx, hby, sprite_size,
                                                 &dveln_visible);

            if (dvelp_visible || !dveln_visible) {
                draw_sprite(dvelp_proj, sprite_size, *delta_positive_sprite,
                            vec4(0.f, cockpit_brightness, 0.f,
                                 dvelp_visible ? 1.f : .3f));
            }
            if (dveln_visible || !dvelp_visible) {
                draw_sprite(dveln_proj, sprite_size, *delta_negative_sprite,
                            vec4(0.f, cockpit_brightness, 0.f,
                                 dveln_visible ? 1.f : .3f));
            }

            if (dvelp_visible) {
                draw_text(dvelp_proj + vec2(0.f, 2.f * sys),
                          vec2(sxs * .5f, sys),
                          localize(t.relative_velocity.length(), 2,
                                   LS_UNIT_M_S),
                          ALIGN_CENTER, ALIGN_BOTTOM);
            }

            if (dveln_visible) {
                draw_text(dveln_proj + vec2(0.f, 2.f * sys),
                          vec2(sxs * .5f, sys),
                          localize(-t.relative_velocity.length(), 2,
                                   LS_UNIT_M_S),
                          ALIGN_CENTER, ALIGN_BOTTOM);
            }

            enum WeaponType wt = player.ship->weapons[0].type;
            const WeaponClass *wc = weapon_classes[wt];

            float p_sqr = t.relative_position.dot(t.relative_position);
            float s_sqr = wc->projectile_velocity * wc->projectile_velocity;
            float v_sqr = t.relative_velocity.dot(t.relative_velocity);
            float np2 = t.relative_position.dot(t.relative_velocity) / s_sqr;
            float nq = (p_sqr + v_sqr) / s_sqr;

            float t2 = np2 + sqrtf(np2 * np2 + nq);
            vec3 aim2 = t.relative_position + t2 * t.relative_velocity;

            aim_proj = project_clamp_to_border(status, aim2, hbx, hby,
                                               sprite_size, &aim_visible);

            draw_sprite(aim_proj, sprite_size, *target_aim_sprite, vec4(0.f, cockpit_brightness, 0.f, aim_visible ? 1.f : .3f));
        }
    }
}


void draw_cockpit(const GraphicsStatus &status, const WorldState &world)
{
    // FIXME
    static float cockpit_brightness = 1.f;

    float brightness_adaption_interval = world.interval;
    // With time speed-up, we need to limit this so we don't get flickering
    if (brightness_adaption_interval > 1.f) {
        brightness_adaption_interval = 1.f;
    }
    cockpit_brightness += .4f * brightness_adaption_interval
                          * (status.luminance - cockpit_brightness);

    if (cockpit_brightness < .2f) {
        cockpit_brightness = .2f;
    }


    gl::framebuffer *main_fb = gl::framebuffer::current();

    cockpit_fb->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_scratches(status, world, main_fb);

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
    line_prg->uniform<vec4>("color") = vec4(0.f, cockpit_brightness, 0.f, 1.f);

    draw_cockpit_controls(world, sxs, sys);


    unsigned hud_height = height * 3 / 4;

    glEnable(GL_SCISSOR_TEST);
    glScissor((width - hud_height) / 2, height - hud_height, hud_height, hud_height);


    vec2 hbx, hby;
    hbx[0] = -.75f * status.height / status.width;
    hbx[1] = -hbx[0];
    hby[0] = -.5f;
    hby[1] = 1.f;


    draw_target_cross(status, world, sxs, sys);

    draw_velocity_indicators(status, world, cockpit_brightness, sxs, sys,
                             hbx, hby);

    draw_orbit_grid(status, world, cockpit_brightness, aspect, sxs, sys,
                    hbx, hby);
    draw_artificial_horizon(status, world, aspect, sxs, sys, hbx, hby);


    draw_radar_contacts(status, world, cockpit_brightness, blink_time, sxs, sys,
                        hbx, hby);


    glDisable(GL_SCISSOR_TEST);


    main_fb->bind();
#ifdef COCKPIT_SUPERSAMPLING
    cockpit_ms_fb->blit();
#else
    cockpit_fb->blit();
#endif
}
