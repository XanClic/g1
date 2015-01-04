#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

#include <dake/dake.hpp>
#include <SDL2/SDL.h>

#include "graphics.hpp"
#include "localize.hpp"
#include "main_loop.hpp"
#include "physics.hpp"
#include "ui.hpp"


using namespace dake::math;


enum MouseDirection {
    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_DOWN,
    MOUSE_UP
};


namespace std
{
template<> struct hash<MouseDirection> {
    size_t operator()(const MouseDirection &d) const { return hash<int>()(d); }
};
}


static SDL_Window *wnd;
static int wnd_width, wnd_height;


static std::unordered_map<SDL_Keycode, std::string> keyboard_mappings;
static std::unordered_map<MouseDirection, std::string> mouse_mappings;


static void create_context(int w, int h, int major = 0, int minor = 0)
{
    if (major > 0) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    wnd = SDL_CreateWindow("g1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!SDL_GL_CreateContext(wnd)) {
        SDL_DestroyWindow(wnd);
        wnd = nullptr;

        throw std::runtime_error(std::string("Could not create OpenGL Core context: ") + SDL_GetError());
    }
}


void init_ui(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    create_context(1280, 720);

    GLint maj, min;
    glGetIntegerv(GL_MAJOR_VERSION, &maj);
    glGetIntegerv(GL_MINOR_VERSION, &min);

    if ((maj < 3) || ((maj == 3) && (min < 3))) {
        SDL_DestroyWindow(wnd);
        wnd = nullptr;

        fprintf(stderr, "Received OpenGL %i.%i Core, 3.3 Core required; "
                "retrying with forced 3.3 Core (thank you based Mesa)\n", maj, min);

        create_context(1280, 720, 3, 3);

        glGetIntegerv(GL_MAJOR_VERSION, &maj);
        glGetIntegerv(GL_MINOR_VERSION, &min);

        if ((maj < 3) || ((maj == 3) && (min < 3))) {
            SDL_DestroyWindow(wnd);
            wnd = nullptr;

            throw std::runtime_error("OpenGL version too old (has " + std::to_string(maj) + "." + std::to_string(min) +
                                     " Core; 3.3 Core is required");
        }
    }

    SDL_GL_SetSwapInterval(1);

    printf("OpenGL %i.%i Core initialized\n", maj, min);


    init_graphics();
    set_resolution(1280, 720);

    wnd_width = 1280;
    wnd_height = 720;


    keyboard_mappings[SDLK_d] = "strafe.-x";
    keyboard_mappings[SDLK_a] = "strafe.+x";
    keyboard_mappings[SDLK_s] = "strafe.-y";
    keyboard_mappings[SDLK_w] = "strafe.+y";
    keyboard_mappings[SDLK_PAGEDOWN] = "strafe.-z";
    keyboard_mappings[SDLK_PAGEUP  ] = "strafe.+z";

    mouse_mappings[MOUSE_DOWN ] = "rotate.-x";
    mouse_mappings[MOUSE_UP   ] = "rotate.+x";
    mouse_mappings[MOUSE_LEFT ] = "rotate.-y";
    mouse_mappings[MOUSE_RIGHT] = "rotate.+y";
    keyboard_mappings[SDLK_q] = "rotate.-z";
    keyboard_mappings[SDLK_e] = "rotate.+z";

    keyboard_mappings[SDLK_z] = "-main_engine";
    keyboard_mappings[SDLK_x] = "+main_engine";
}


void ui_process_events(Input &input)
{
    static bool capture_movement;

    if (capture_movement) {
        int abs_x, abs_y;
        SDL_GetMouseState(&abs_x, &abs_y);

        float rel_x = 2.f * abs_x / wnd_width - 1.f;
        float rel_y = 1.f - 2.f * abs_y / wnd_height;

        input.mapping_states[mouse_mappings[MOUSE_LEFT ]] = dake::helper::maximum(-rel_x, 0.f);
        input.mapping_states[mouse_mappings[MOUSE_RIGHT]] = dake::helper::maximum( rel_x, 0.f);
        input.mapping_states[mouse_mappings[MOUSE_DOWN ]] = dake::helper::maximum(-rel_y, 0.f);
        input.mapping_states[mouse_mappings[MOUSE_UP   ]] = dake::helper::maximum( rel_y, 0.f);
    } else {
        for (const auto &mm: mouse_mappings) {
            input.mapping_states[mm.second] = 0.f;
        }
    }

    for (const auto &km: keyboard_mappings) {
        input.mapping_states[km.second] = 0.f;
    }


    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quit_main_loop();
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                       set_resolution(event.window.data1, event.window.data2);
                       wnd_width  = event.window.data1;
                       wnd_height = event.window.data2;
                       break;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    capture_movement = true;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    capture_movement = false;
                }
                break;

            case SDL_KEYDOWN: {
                auto mapping = keyboard_mappings.find(event.key.keysym.sym);
                if (mapping != keyboard_mappings.end()) {
                    input.mapping_states[mapping->second] = 1.f;
                } else {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit_main_loop();
                            break;

                        case SDLK_F12:
                            olo = static_cast<Localization>((olo + 1) % LOCALIZATIONS);
                    }
                }
                break;
            }
        }
    }
}


void ui_process_menu_events(bool &quit, bool &mouse_down, dake::math::vec2 &mouse_pos)
{
    int abs_x, abs_y;
    mouse_down = SDL_GetMouseState(&abs_x, &abs_y) & SDL_BUTTON(SDL_BUTTON_LEFT);

    mouse_pos.x() = 2.f * abs_x / wnd_width - 1.f;
    mouse_pos.y() = 1.f - 2.f * abs_y / wnd_height;


    SDL_Event event;
    quit = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        set_resolution(event.window.data1, event.window.data2);
                        wnd_width  = event.window.data1;
                        wnd_height = event.window.data2;
                        break;
                }
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;

                    case SDLK_F12:
                        olo = static_cast<Localization>((olo + 1) % LOCALIZATIONS);
                }
                break;
        }
    }
}


void ui_swap_buffers(void)
{
    SDL_GL_SwapWindow(wnd);
}
