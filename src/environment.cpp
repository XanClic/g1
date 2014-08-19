#include <cassert>
#include <dake/dake.hpp>

#include "environment.hpp"
#include "graphics.hpp"
#include "xsmd.hpp"


using namespace dake;
using namespace dake::math;


static XSMD *earth;
static gl::texture *earth_tex, *cloud_tex;
static gl::program *earth_prg, *cloud_prg, *atmob_prg, *atmof_prg, *sun_prg;
static gl::framebuffer *sub_atmo_fbo;
static gl::vertex_array *sun_va;
static mat4 earth_mv, cloud_mv, atmo_mv;


static void resize(unsigned w, unsigned h);


void init_environment(void)
{
    earth = load_xsmd("assets/earth.xsmd");
    earth->make_vertex_array();

    earth_tex = new gl::texture("assets/earth.png");
    cloud_tex = new gl::texture("assets/clouds.png");

    earth_tex->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

    cloud_tex->bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);


    sun_va = new gl::vertex_array;
    sun_va->set_elements(4);

    vec2 sun_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    gl::vertex_attrib *sun_pos = sun_va->attrib(0);
    sun_pos->format(2);
    sun_pos->data(sun_vertex_positions);
    sun_pos->load();


    earth_mv = mat4::identity().scale(vec3(6378.f, 6357.f, 6378.f));
    cloud_mv = mat4::identity().scale(vec3(6388.f, 6367.f, 6388.f));
    atmo_mv  = mat4::identity().scale(vec3(6478.f, 6457.f, 6478.f));


    earth_prg = new gl::program {gl::shader::vert("assets/ptn_vert.glsl"),   gl::shader::frag("assets/earth_frag.glsl")};
    cloud_prg = new gl::program {gl::shader::vert("assets/ptn_vert.glsl"),   gl::shader::frag("assets/cloud_frag.glsl")};
    atmob_prg = new gl::program {gl::shader::vert("assets/ptn_vert.glsl"),   gl::shader::frag("assets/atmob_frag.glsl")};
    atmof_prg = new gl::program {gl::shader::vert("assets/atmof_vert.glsl"), gl::shader::frag("assets/atmof_frag.glsl")};

    earth->bind_program_vertex_attribs(*earth_prg);
    earth->bind_program_vertex_attribs(*cloud_prg);
    earth->bind_program_vertex_attribs(*atmob_prg);
    earth->bind_program_vertex_attribs(*atmof_prg);

    earth_prg->bind_frag("out_col", 0);
    cloud_prg->bind_frag("out_col", 0);
    atmob_prg->bind_frag("out_col", 0);
    atmof_prg->bind_frag("out_col", 0);


    sun_prg = new gl::program {gl::shader::vert("assets/sun_vert.glsl"), gl::shader::frag("assets/sun_frag.glsl")};
    sun_prg->bind_attrib("va_position", 0);
    sun_prg->bind_frag("out_col", 0);


    sub_atmo_fbo = new gl::framebuffer(1);
    sub_atmo_fbo->depth().tmu() = 1;


    register_resize_handler(resize);
}


static void resize(unsigned w, unsigned h)
{
    sub_atmo_fbo->resize(w, h);
}


void draw_environment(const GraphicsStatus &status)
{
    static float step = static_cast<float>(M_PI);

    vec3 light_dir = vec3(sinf(step), 0.f, cosf(step));
    step -= .001f;


    float sa_zn = (status.camera_position.length() - 6400.f) / 1.5f;
    float sa_zf =  status.camera_position.length() + 7000.f;

    mat4 sa_proj = mat4::projection(status.yfov, static_cast<float>(status.width) / status.height, sa_zn, sa_zf);


    vec4 sun_pos = 149.6e6f * -light_dir;
    sun_pos.w() = 1.f;
    sun_pos = status.world_to_camera * sun_pos;

    vec4 projected_sun = status.projection * sun_pos;
    projected_sun /= projected_sun.w();

    if (sun_pos.z() < 0.f) {
        float sun_radius = atanf(696.e3f / sun_pos.length()) * 2.f / status.yfov;

        // artificial correction (pre-blur)
        sun_radius *= 3.f;

        // everything is in front of the sun
        glDisable(GL_DEPTH_TEST);

        sun_prg->use();
        sun_prg->uniform<vec2>("sun_position") = projected_sun;
        sun_prg->uniform<vec2>("sun_size") = vec2(sun_radius * status.height / status.width, sun_radius);

        sun_va->draw(GL_TRIANGLE_STRIP);

        glEnable(GL_DEPTH_TEST);
    }


    gl::framebuffer *main_fbo = gl::framebuffer::current();

    sub_atmo_fbo->bind();
    glClearDepth(0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearDepth(1.f);


    glDepthFunc(GL_ALWAYS);
    glCullFace(GL_FRONT);

    atmo_mv.rotate(.0004f, vec3(0.f, 1.f, 0.f));

    atmob_prg->use();
    atmob_prg->uniform<mat4>("mat_mv") = atmo_mv;
    atmob_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;

    earth->draw();


    glDepthFunc(GL_LEQUAL);
    glCullFace(GL_BACK);

    earth_tex->bind();

    earth_mv.rotate(.0004f, vec3(0.f, 1.f, 0.f));

    earth_prg->use();
    earth_prg->uniform<mat4>("mat_mv") = earth_mv;
    earth_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;
    earth_prg->uniform<mat3>("mat_nrm") = mat3(earth_mv).transposed_inverse();
    earth_prg->uniform<vec3>("cam_pos") = status.camera_position;
    earth_prg->uniform<vec3>("light_dir") = light_dir;
    earth_prg->uniform<gl::texture>("tex") = *earth_tex;

    earth->draw();


    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    cloud_mv.rotate(.0004f, vec3(0.f, 1.f, 0.f));

    cloud_tex->bind();

    cloud_prg->use();
    cloud_prg->uniform<mat4>("mat_mv") = cloud_mv;
    cloud_prg->uniform<mat4>("mat_proj") = sa_proj * status.world_to_camera;
    cloud_prg->uniform<mat3>("mat_nrm") = mat3(cloud_mv).transposed_inverse();
    cloud_prg->uniform<vec3>("light_dir") = light_dir;
    cloud_prg->uniform<gl::texture>("tex") = *cloud_tex;

    earth->draw();

    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);


    main_fbo->bind();

    (*sub_atmo_fbo)[0].bind();
    sub_atmo_fbo->depth().bind();

    atmof_prg->use();
    atmof_prg->uniform<mat4>("mat_mv") = atmo_mv;
    atmof_prg->uniform<mat4>("mat_proj") = status.projection * status.world_to_camera;
    atmof_prg->uniform<mat3>("mat_nrm") = mat3(atmo_mv).transposed_inverse();
    atmof_prg->uniform<vec3>("cam_pos") = status.camera_position;
    atmof_prg->uniform<vec3>("cam_fwd") = status.camera_forward;
    atmof_prg->uniform<vec3>("light_dir") = light_dir;
    atmof_prg->uniform<vec2>("sa_z_dim") = vec2(sa_zn, sa_zf);
    atmof_prg->uniform<vec2>("screen_dim") = vec2(status.width, status.height);
    atmof_prg->uniform<gl::texture>("color") = (*sub_atmo_fbo)[0];
    atmof_prg->uniform<gl::texture>("depth") = sub_atmo_fbo->depth();

    earth->draw();
}
