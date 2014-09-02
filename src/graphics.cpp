#include <cmath>
#include <dake/dake.hpp>

#include "cockpit.hpp"
#include "environment.hpp"
#include "graphics.hpp"
#include "physics.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


static GraphicsStatus status;

static std::vector<void (*)(unsigned w, unsigned h)> resize_handlers;

static gl::framebuffer *main_fb, *bloom_fbs[2];
gl::vertex_array *quad_vertices;
static gl::program *fb_combine_prg, *high_pass_prg, *blur_prgs[4];


void init_graphics(void)
{
    gl::glext_init();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_DEPTH_CLAMP);
    glEnable(GL_BLEND);

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepthf(1.f);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    main_fb = new gl::framebuffer(1, GL_R11F_G11F_B10F);
    (*main_fb)[0].filter(GL_LINEAR);

    (*main_fb)[0].bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, vec4::zero());

    for (gl::framebuffer *&bloom_fb: bloom_fbs) {
        bloom_fb = new gl::framebuffer(1, GL_R11F_G11F_B10F, gl::framebuffer::NO_DEPTH_OR_STENCIL);
        (*bloom_fb)[0].set_tmu(1);
        (*bloom_fb)[0].filter(GL_LINEAR);

        (*bloom_fb)[0].bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, vec4::zero());
    }

    quad_vertices = new gl::vertex_array;
    quad_vertices->set_elements(4);

    vec2 fb_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    quad_vertices->attrib(0)->format(2);
    quad_vertices->attrib(0)->data(fb_vertex_positions);
    quad_vertices->attrib(0)->load();


    gl::shader fb_vert_sh(gl::shader::VERTEX, "assets/fb_vert.glsl");

    fb_combine_prg = new gl::program {gl::shader::frag("assets/fb_frag.glsl")};
    high_pass_prg  = new gl::program {gl::shader::frag("assets/high_pass_frag.glsl")};

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
            case 0: *blur_prgs[i] << gl::shader::frag("assets/blur0_x_frag.glsl"); break;
            case 1: *blur_prgs[i] << gl::shader::frag("assets/blur0_y_frag.glsl"); break;
            case 2: *blur_prgs[i] << gl::shader::frag("assets/blurn_x_frag.glsl"); break;
            case 3: *blur_prgs[i] << gl::shader::frag("assets/blurn_y_frag.glsl"); break;
        }

        blur_prgs[i]->bind_attrib("in_pos", 0);
        blur_prgs[i]->bind_frag("out_col", 0);
    }


    status.z_near =  .01f;
    status.z_far  = 500.f;

    status.yfov   = static_cast<float>(M_PI) / 4.f;


    init_environment();
    init_cockpit();
}


void set_resolution(unsigned width, unsigned height)
{
    status.projection = mat4::projection(status.yfov, static_cast<float>(width) / height, status.z_near, status.z_far);
    glViewport(0, 0, width, height);

    status.width  = width;
    status.height = height;

    main_fb->resize(width, height);

    for (gl::framebuffer *&bloom_fb: bloom_fbs) {
        bloom_fb->resize(width / 2, height / 2);
    }

    for (void (*rh)(unsigned w, unsigned h): resize_handlers) {
        rh(width, height);
    }
}


void register_resize_handler(void (*rh)(unsigned w, unsigned h))
{
    resize_handlers.push_back(rh);
}


static void calculate_camera(mat4 &mat, const vec3 &pos, const vec3 &forward, const vec3 &up)
{
    vec3 f = forward.normalized();
    vec3 u = up.normalized();
    vec3 r = f.cross(u).normalized();

    u = r.cross(f);

    mat[0][0] =  r[0];
    mat[0][1] =  u[0];
    mat[0][2] = -f[0];
    mat[0][3] = 0.f;

    mat[1][0] =  r[1];
    mat[1][1] =  u[1];
    mat[1][2] = -f[1];
    mat[1][3] = 0.f;

    mat[2][0] =  r[2];
    mat[2][1] =  u[2];
    mat[2][2] = -f[2];
    mat[2][3] = 0.f;

    mat[3][0] = 0.f;
    mat[3][1] = 0.f;
    mat[3][2] = 0.f;
    mat[3][3] = 1.f;

    mat.translate(-pos);
}


void do_graphics(const WorldState &input)
{
    status.camera_position = input.player_position;
    status.camera_forward  = input.player_forward;

    calculate_camera(status.world_to_camera, status.camera_position, input.player_forward, input.player_up);


    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);


    main_fb->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_environment(status, input);

    glDisable(GL_DEPTH_TEST);

    draw_cockpit(status, input);


    glDisable(GL_BLEND);


    bloom_fbs[0]->bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, status.width / 2, status.height / 2);

    (*main_fb)[0].bind();

    high_pass_prg->use();
    high_pass_prg->uniform<gl::texture>("fb") = (*main_fb)[0];

    quad_vertices->draw(GL_TRIANGLE_STRIP);

    for (int i = 7; i >= 0; i--) {
        float mag = exp2f(i);

        for (int cur_fb: {0, 1}) {
            gl::program *blur_prg = blur_prgs[(i == 7 ? 0 : 2) + cur_fb];

            bloom_fbs[!cur_fb]->bind();

            (*bloom_fbs[cur_fb])[0].bind();

            blur_prg->use();
            blur_prg->uniform<float>("epsilon") = mag / (cur_fb ? 512.f : 512.f * status.width / status.height);
            blur_prg->uniform<gl::texture>("tex") = (*bloom_fbs[cur_fb])[0];

            quad_vertices->draw(GL_TRIANGLE_STRIP);
        }
    }


    gl::framebuffer::unbind();
    glViewport(0, 0, status.width, status.height);

    (*main_fb)[0].bind();
    (*bloom_fbs[0])[0].bind();

    fb_combine_prg->use();
    fb_combine_prg->uniform<gl::texture>("fb") = (*main_fb)[0];
    fb_combine_prg->uniform<gl::texture>("bloom") = (*bloom_fbs[0])[0];

    quad_vertices->draw(GL_TRIANGLE_STRIP);


    ui_swap_buffers();
}
