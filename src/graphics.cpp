#include <cmath>
#include <dake/dake.hpp>

#include "environment.hpp"
#include "graphics.hpp"
#include "physics.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


static GraphicsStatus status;

static std::vector<void (*)(unsigned w, unsigned h)> resize_handlers;

static gl::framebuffer *main_fbo;
static gl::vertex_array *fb_vertices;
static gl::program *fb_pass_prg;


void init_graphics(void)
{
    gl::glext_init();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepthf(1.f);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    main_fbo = new gl::framebuffer(1);

    fb_vertices = new gl::vertex_array;
    fb_vertices->set_elements(4);

    vec2 fb_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    fb_vertices->attrib(0)->format(2);
    fb_vertices->attrib(0)->data(fb_vertex_positions);
    fb_vertices->attrib(0)->load();


    fb_pass_prg = new gl::program {gl::shader::vert("assets/fb_vert.glsl"), gl::shader::frag("assets/fb_frag.glsl")};
    fb_pass_prg->bind_attrib("in_pos", 0);
    fb_pass_prg->bind_frag("out_col", 0);


    status.z_near =   100.f;
    status.z_far  = 50000.f;


    init_environment();
}


void set_resolution(unsigned width, unsigned height)
{
    status.projection = mat4::projection(static_cast<float>(M_PI) / 4.f, static_cast<float>(width) / height, status.z_near, status.z_far);
    glViewport(0, 0, width, height);

    status.width  = width;
    status.height = height;

    main_fbo->resize(width, height);

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
    status.camera_position = vec3(0.f, 5000.f, 18000.f);
    calculate_camera(status.world_to_camera, status.camera_position, vec3(0.f, -.28f, -1.f), vec3(0.f, 1.f, 0.f));


    main_fbo->bind();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_environment(status);


    gl::framebuffer::unbind();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    (*main_fbo)[0].bind();

    fb_pass_prg->use();
    fb_pass_prg->uniform<gl::texture>("fb") = (*main_fbo)[0];

    fb_vertices->draw(GL_TRIANGLE_STRIP);


    ui_swap_buffers();
}
