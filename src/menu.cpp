#include <dake/dake.hpp>

#include "localize.hpp"
#include "menu.hpp"
#include "text.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


enum Buttons {
    NONE,

    TEST_ENV,
};


static gl::texture *menu_bg;
static gl::program *menu_prg;
static gl::vertex_array *menu_va;
static unsigned width, height;
static float menu_bg_aspect;

static Buttons hover, down;


static void render_menu(bool loading = false);


void menu_loop(void)
{
    bool quit = false, to_game = false;
    bool mouse_down = false, mouse_was_down;
    vec2 mouse_pos;

    gl::image menu_bg_img("assets/menu_bg.png");
    menu_bg_aspect = static_cast<float>(menu_bg_img.width()) / menu_bg_img.height();
    menu_bg = new gl::texture(menu_bg_img);

    menu_prg = new gl::program {gl::shader::vert("shaders/menu_vert.glsl"),
                                gl::shader::frag("shaders/menu_frag.glsl")};

    menu_prg->bind_attrib("va_pos", 0);
    menu_prg->bind_frag("out_col", 0);

    menu_va = new gl::vertex_array;
    menu_va->set_elements(4);

    vec2 mva_vertex_positions[] = {
        vec2(-1.f, 1.f), vec2(-1.f, -1.f), vec2(1.f, 1.f), vec2(1.f, -1.f)
    };

    menu_va->attrib(0)->format(2);
    menu_va->attrib(0)->data(mva_vertex_positions);


    while (!quit && !to_game) {
        mouse_was_down = mouse_down;
        ui_process_menu_events(quit, mouse_down, mouse_pos);

        hover = NONE; down = NONE;
        if ((mouse_pos.x() > -.5f) && (mouse_pos.x() < .5f) && (mouse_pos.y() > -.95f) && (mouse_pos.y() < -.65f)) {
            (mouse_down ? down : hover) = TEST_ENV;
        }

        if (mouse_was_down && !mouse_down && (hover == TEST_ENV)) {
            to_game = true;
        }

        render_menu();
    }

    render_menu(true);

    delete menu_bg;
    delete menu_prg;
    delete menu_va;

    if (quit) {
        exit(0);
    }
}


static void render_menu(bool loading)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    menu_bg->bind();

    menu_prg->use();
    menu_prg->uniform<gl::texture>("menu_bg") = *menu_bg;

    vec2 menu_bg_size = vec2(menu_bg_aspect * height / width, 1.f);
    if (menu_bg_size.x() > 1.f) {
        menu_bg_size /= static_cast<float>(menu_bg_size.x());
    }

    menu_prg->uniform<vec2>("size") = menu_bg_size;

    menu_va->draw(GL_TRIANGLE_STRIP);


    if (!loading) {
        set_text_color((hover == TEST_ENV) ? vec4(1.f, 1.f, 1.f, 1.f)
                     : (down  == TEST_ENV) ? vec4(.3f, .3f, .3f, 1.f)
                                           : vec4(.6f, .6f, .6f, 1.f));

        draw_text(vec2(0.f, -.9f), vec2(.1f * height / width, .2f), localize(LS_TEST_ENVIRONMENT),
                  ALIGN_CENTER, ALIGN_BOTTOM);
    } else {
        set_text_color(vec4(.5f, .5f, .5f, 1.f));

        draw_text(vec2(0.f, -.9f), vec2(.1f * height / width, .2f), localize(LS_LOADING),
                  ALIGN_CENTER, ALIGN_BOTTOM);
    }


    ui_swap_buffers();
}


void menu_set_resolution(unsigned w, unsigned h)
{
    glViewport(0, 0, w, h);

    width = w;
    height = h;
}
