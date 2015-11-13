#include <cmath>
#include <dake/dake.hpp>

#include "cockpit.hpp"
#include "environment.hpp"
#include "graphics.hpp"
#include "menu.hpp"
#include "options.hpp"
#include "particles.hpp"
#include "physics.hpp"
#include "text.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


static GraphicsStatus status;
static unsigned change_width, change_height;

static std::vector<void (*)(unsigned w, unsigned h)> resize_handlers;

gl::vertex_array *quad_vertices;

static gl::framebuffer *main_fb, *bloom_fbs[2], *avg_fb;
static gl::program *fb_combine_prg, *high_pass_prg, *blur_prgs[4], *avg_prg;

static bool in_menu;


void init_graphics(void)
{
    gl::glext_init();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_CLAMP);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepthf(1.f);


    init_text();


    in_menu = true;
}


void init_game_graphics(void)
{
    init_environment();
    init_cockpit();
    init_particles();


    in_menu = false;


    main_fb = new gl::framebuffer(1, GL_R11F_G11F_B10F);
    (*main_fb)[0].filter(GL_LINEAR);
    (*main_fb)[0].wrap(GL_CLAMP_TO_BORDER);
    (*main_fb)[0].set_border_color(vec4::zero());

    if (global_options.bloom_type > Options::NO_BLOOM) {
        for (gl::framebuffer *&bloom_fb: bloom_fbs) {
            bloom_fb = new gl::framebuffer(1, GL_R11F_G11F_B10F,
                                           gl::framebuffer::NO_DEPTH_OR_STENCIL);
            (*bloom_fb)[0].set_tmu(1);
            (*bloom_fb)[0].filter(GL_LINEAR);
            (*bloom_fb)[0].wrap(GL_CLAMP_TO_BORDER);
            (*bloom_fb)[0].set_border_color(vec4::zero());
        }
    }

    avg_fb = new gl::framebuffer(1, GL_R32F, gl::framebuffer::NO_DEPTH_OR_STENCIL);
    avg_fb->resize(256, 256);
    (*avg_fb)[0].filter(GL_LINEAR);



    quad_vertices = new gl::vertex_array;
    quad_vertices->set_elements(4);

    vec2 fb_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    quad_vertices->attrib(0)->format(2);
    quad_vertices->attrib(0)->data(fb_vertex_positions);


    gl::shader fb_vert_sh(gl::shader::VERTEX, "shaders/fb_vert.glsl");

    if (global_options.bloom_type == Options::NO_BLOOM) {
        fb_combine_prg = new gl::program
                         {gl::shader::frag("shaders/fbnb_frag.glsl")};
    } else {
        fb_combine_prg = new gl::program
                         {gl::shader::frag("shaders/fb_frag.glsl")};
    }
    high_pass_prg = new gl::program
                    {gl::shader::frag("shaders/high_pass_frag.glsl")};

    *fb_combine_prg << fb_vert_sh;
    *high_pass_prg  << fb_vert_sh;

    fb_combine_prg->bind_attrib("in_pos", 0);
    high_pass_prg ->bind_attrib("in_pos", 0);

    fb_combine_prg->bind_frag("out_col", 0);
    high_pass_prg ->bind_frag("out_col", 0);

    for (int i = 0; i < 4; i++) {
        blur_prgs[i] = new gl::program;
        *blur_prgs[i] << fb_vert_sh;

        switch (i) {
            case 0: *blur_prgs[i] << gl::shader::frag("shaders/blur0_x_frag.glsl"); break;
            case 1: *blur_prgs[i] << gl::shader::frag("shaders/blur0_y_frag.glsl"); break;
            case 2: *blur_prgs[i] << gl::shader::frag("shaders/blurn_x_frag.glsl"); break;
            case 3: *blur_prgs[i] << gl::shader::frag("shaders/blurn_y_frag.glsl"); break;
        }

        blur_prgs[i]->bind_attrib("in_pos", 0);
        blur_prgs[i]->bind_frag("out_col", 0);
    }


    if (global_options.bloom_type == Options::NO_BLOOM) {
        avg_prg = new gl::program {gl::shader::frag("shaders/avgnb_frag.glsl")};
    } else {
        avg_prg = new gl::program {gl::shader::frag("shaders/avg_frag.glsl")};
    }
    *avg_prg << fb_vert_sh;
    avg_prg->bind_attrib("in_pos", 0);
    avg_prg->bind_frag("out_value", 0);


    status.z_near = 1.f;
    status.z_far  = 500.e3f;

    status.yfov   = static_cast<float>(M_PI) / 3.f;

    status.luminance = 1.f;
}


void set_resolution(unsigned width, unsigned height)
{
    change_width  = width;
    change_height = height;

    if (in_menu) {
        menu_set_resolution(width, height);
    }
}


void update_resolution(void)
{
    status.width  = change_width;
    status.height = change_height;

    status.aspect = static_cast<float>(change_width) / change_height;

    status.projection = fmat4::projection(status.yfov, status.aspect,
                                          status.z_near, status.z_far);
    glViewport(0, 0, change_width, change_height);

    main_fb->resize(change_width, change_height);

    for (gl::framebuffer *&bloom_fb: bloom_fbs) {
        switch (global_options.bloom_type) {
            case Options::NO_BLOOM:
                break;

            case Options::LQ_BLOOM:
                bloom_fb->resize(change_width / 4, change_height / 4);
                break;

            case Options::HQ_BLOOM:
                bloom_fb->resize(change_width, change_height);
                break;
        }
    }

    for (void (*rh)(unsigned w, unsigned h): resize_handlers) {
        rh(change_width, change_height);
    }

    glLineWidth(change_height / 500.f);

    change_width = change_height = 0;
}


void register_resize_handler(void (*rh)(unsigned w, unsigned h))
{
    resize_handlers.push_back(rh);
}


static void calculate_camera(fmat4 &mat, const fvec3 &pos, const fvec3 &forward,
                             const fvec3 &up)
{
    fvec3 f = forward.normalized();
    fvec3 u = up.normalized();
    fvec3 r = f.cross(u).normalized();

    u = r.cross(f);

    mat[0] = fvec4(r[0], u[0], -f[0], 0.f);
    mat[1] = fvec4(r[1], u[1], -f[1], 0.f);
    mat[2] = fvec4(r[2], u[2], -f[2], 0.f);
    mat[3] = fvec4( 0.f,  0.f,   0.f, 1.f);

    mat.translate(-pos);
}


static void do_bloom(void)
{
    bloom_fbs[0]->bind();
    if (global_options.bloom_type == Options::HQ_BLOOM) {
        glViewport(0, 0, status.width, status.height);
    } else {
        glViewport(0, 0, status.width / 4, status.height / 4);
    }

    (*main_fb)[0].bind();

    high_pass_prg->use();
    high_pass_prg->uniform<float>("factor") = .7f / status.luminance;
    high_pass_prg->uniform<gl::texture>("fb") = (*main_fb)[0];

    quad_vertices->draw(GL_TRIANGLE_STRIP);

    int low_i = (global_options.bloom_type == Options::HQ_BLOOM) ? 0 : 3;

    for (int i = 5; i >= low_i; i--) {
        float mag = exp2f(i);

        for (int cur_fb: {0, 1}) {
            gl::program *blur_prg = blur_prgs[(i == 5 ? 0 : 2) + cur_fb];

            bloom_fbs[!cur_fb]->bind();

            (*bloom_fbs[cur_fb])[0].bind();

            blur_prg->use();
            blur_prg->uniform<float>("epsilon") =
                mag / (cur_fb ? 1024.f : 1024.f * status.aspect);
            blur_prg->uniform<gl::texture>("tex") = (*bloom_fbs[cur_fb])[0];

            quad_vertices->draw(GL_TRIANGLE_STRIP);
        }
    }
}


void do_graphics(const WorldState &input)
{
    if (change_width && change_height) {
        update_resolution();
    }


    const ShipState &ps = input.ships[input.player_ship];

    status.camera_position = ps.position
                           + fmat3(ps.right, ps.up, -ps.forward) *
                             ps.ship->cockpit_position;

    status.camera_forward  = ps.forward;

    calculate_camera(status.world_to_camera,
                     (fvec3)status.camera_position,
                     status.camera_forward, ps.up);
    calculate_camera(status.relative_to_camera, fvec3::zero(),
                     status.camera_forward, ps.up);


    status.time_speed_up = input.time_speed_up;


    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    main_fb->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_environment(status, input);

    draw_particles(status, input.particles);

    glDisable(GL_DEPTH_TEST);

    draw_cockpit(status, input);


    glDisable(GL_BLEND);


    if (global_options.bloom_type > Options::NO_BLOOM) {
        do_bloom();
    }


    avg_fb->bind();
    glViewport(0, 0, 256, 256);

    (*main_fb)[0].bind();
    if (global_options.bloom_type > Options::NO_BLOOM) {
        (*bloom_fbs[0])[0].bind();
    }

    avg_prg->use();
    avg_prg->uniform<gl::texture>("tex") = (*main_fb)[0];
    if (global_options.bloom_type > Options::NO_BLOOM) {
        avg_prg->uniform<gl::texture>("bloom") = (*bloom_fbs[0])[0];
    }

    quad_vertices->draw(GL_TRIANGLE_STRIP);

    gl::framebuffer::unbind();


    (*avg_fb)[0].bind();
    glGenerateMipmap(GL_TEXTURE_2D);

    float *pre_avg = new float[256];
    glGetTexImage(GL_TEXTURE_2D, 4, GL_RED, GL_FLOAT, pre_avg);

    float highest_avg = -HUGE_VALF;
    for (unsigned i = 0; i < 256; i++) {
        highest_avg = helper::maximum(highest_avg, pre_avg[i]);
    }
    highest_avg = expf(highest_avg);
    delete[] pre_avg;

    highest_avg = helper::maximum(highest_avg, .05f);


    glViewport(0, 0, status.width, status.height);

    (*main_fb)[0].bind();
    if (global_options.bloom_type > Options::NO_BLOOM) {
        (*bloom_fbs[0])[0].bind();
    }

    fb_combine_prg->use();
    fb_combine_prg->uniform<float>("factor") = .7f / status.luminance;
    fb_combine_prg->uniform<gl::texture>("fb") = (*main_fb)[0];
    if (global_options.bloom_type > Options::NO_BLOOM) {
        fb_combine_prg->uniform<gl::texture>("bloom") = (*bloom_fbs[0])[0];
    }

    quad_vertices->draw(GL_TRIANGLE_STRIP);


    ui_swap_buffers();


    status.luminance += input.real_interval * (highest_avg - status.luminance);

    if (status.luminance > 2.f) {
        status.luminance = 2.f;
    }
}
