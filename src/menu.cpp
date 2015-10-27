#include <stdexcept>
#include <string>

#include <dake/dake.hpp>

#include "localize.hpp"
#include "menu.hpp"
#include "text.hpp"
#include "ui.hpp"


using namespace dake;
using namespace dake::math;


enum Buttons {
    NONE,

    TEST_ENVIRONMENT,
    TEST_SCENARIO,
};


static gl::texture *menu_bg;
static gl::program *menu_prg;
static gl::vertex_array *menu_va;
static unsigned width, height;
static float menu_bg_aspect;

static Buttons hover, down;


static void render_menu(Buttons loading = NONE);


std::string menu_loop(void)
{
    bool quit = false;
    Buttons to_game = NONE;
    bool mouse_down = false, mouse_was_down;
    fvec2 mouse_pos;

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
        if (mouse_pos.x() < 0.f && mouse_pos.y() > -.95f && mouse_pos.y() < -.65f) {
            (mouse_down ? down : hover) = TEST_ENVIRONMENT;
        } else if (mouse_pos.x() > 0.f && mouse_pos.y() > -.95f && mouse_pos.y() < -.65f) {
            (mouse_down ? down : hover) = TEST_SCENARIO;
        }

        if (mouse_was_down && !mouse_down && hover != NONE) {
            to_game = hover;
        }

        render_menu();
    }

    render_menu(to_game);

    delete menu_bg;
    delete menu_prg;
    delete menu_va;

    if (quit) {
        exit(0);
    }

    if (to_game == TEST_ENVIRONMENT) {
        return "null";
    } else if (to_game == TEST_SCENARIO) {
        return "0";
    } else {
        throw std::runtime_error("Bad to_game value");
    }
}


static void render_menu(Buttons loading)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    menu_bg->bind();

    menu_prg->use();
    menu_prg->uniform<gl::texture>("menu_bg") = *menu_bg;

    fvec2 menu_bg_size = fvec2(menu_bg_aspect * height / width, 1.f);
    if (menu_bg_size.x() > 1.f) {
        menu_bg_size /= static_cast<float>(menu_bg_size.x());
    }

    menu_prg->uniform<fvec2>("size") = menu_bg_size;

    menu_va->draw(GL_TRIANGLE_STRIP);


    if (loading == NONE) {
        set_text_color((hover == TEST_ENVIRONMENT) ? fvec4(1.f, 1.f, 1.f, 1.f)
                     : (down  == TEST_ENVIRONMENT) ? fvec4(.3f, .3f, .3f, 1.f)
                                                   : fvec4(.6f, .6f, .6f, 1.f));

        draw_text(fvec2(-.9f, -.9f), fvec2(.1f * height / width, .2f),
                  localize(LS_TEST_ENVIRONMENT), ALIGN_LEFT, ALIGN_BOTTOM);

        set_text_color((hover == TEST_SCENARIO) ? fvec4(1.f, 1.f, 1.f, 1.f)
                     : (down  == TEST_SCENARIO) ? fvec4(.3f, .3f, .3f, 1.f)
                                                : fvec4(.6f, .6f, .6f, 1.f));

        draw_text(fvec2(.9f, -.9f), fvec2(.1f * height / width, .2f),
                  localize(LS_TEST_SCENARIO), ALIGN_RIGHT, ALIGN_BOTTOM);
    } else {
        set_text_color(fvec4(.5f, .5f, .5f, 1.f));

        draw_text(fvec2(-.9f, -.9f), fvec2(.1f * height / width, .2f),
                  localize(loading == TEST_ENVIRONMENT ? LS_LOADING
                                                       : LS_TEST_ENVIRONMENT),
                  ALIGN_LEFT, ALIGN_BOTTOM);

        draw_text(fvec2(.9f, -.9f), fvec2(.1f * height / width, .2f),
                  localize(loading == TEST_SCENARIO ? LS_LOADING
                                                    : LS_TEST_SCENARIO),
                  ALIGN_RIGHT, ALIGN_BOTTOM);
    }


    ui_swap_buffers();
}


void menu_set_resolution(unsigned w, unsigned h)
{
    glViewport(0, 0, w, h);

    width = w;
    height = h;
}
