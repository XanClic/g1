#include <cstdio>
#include <stdexcept>
#include <string>

#include <dake/dake.hpp>
#include <SDL2/SDL.h>

#include "graphics.hpp"
#include "main_loop.hpp"
#include "physics.hpp"
#include "ui.hpp"


using namespace dake::math;


static SDL_Window *wnd;
static int wnd_width, wnd_height;


static void create_context(int w, int h, int major = 0, int minor = 0)
{
    if (major > 0) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    wnd = SDL_CreateWindow("g1", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

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

        fprintf(stderr, "Received OpenGL %i.%i Core, 3.3 Core required; retrying with forced 3.3 Core (thank you based Mesa)\n", maj, min);

        create_context(1280, 720, 3, 3);

        glGetIntegerv(GL_MAJOR_VERSION, &maj);
        glGetIntegerv(GL_MINOR_VERSION, &min);

        if ((maj < 3) || ((maj == 3) && (min < 3))) {
            SDL_DestroyWindow(wnd);
            wnd = nullptr;

            throw std::runtime_error("OpenGL version too old (has " + std::to_string(maj) + "." + std::to_string(min) + " Core; 3.3 Core is required");
        }
    }

    SDL_GL_SetSwapInterval(1);

    printf("OpenGL %i.%i Core initialized\n", maj, min);


    init_graphics();
    set_resolution(1280, 720);

    wnd_width = 1280;
    wnd_height = 720;
}


void ui_process_events(Input &input)
{
    static bool capture_movement, accel_fwd, accel_bwd, roll_left, roll_right;
    static bool strafe_left, strafe_right, strafe_up, strafe_down;;

    input.yaw   = 0.f;
    input.pitch = 0.f;
    input.roll  = 0.f;

    input.strafe_x = 0.f;
    input.strafe_y = 0.f;
    input.strafe_z = 0.f;

    input.main_engine = 0.f;

    if (capture_movement) {
        int abs_x, abs_y;
        SDL_GetMouseState(&abs_x, &abs_y);

        float rel_x = 2.f * abs_x / wnd_width - 1.f;
        float rel_y = 1.f - 2.f * abs_y / wnd_height;

        input.yaw   = rel_x;
        input.pitch = rel_y;
    }

    if (accel_fwd ^ accel_bwd) {
        input.main_engine = accel_fwd ? 1.f : -1.f;
    }

    if (roll_left ^ roll_right) {
        input.roll = roll_right ? 1.f : -1.f;
    }

    if (strafe_left ^ strafe_right) {
        input.strafe_x = strafe_right ? 1.f : -1.f;
    }

    if (strafe_up ^ strafe_down) {
        input.strafe_y = strafe_up ? 1.f : -1.f;
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
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    capture_movement = true;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    capture_movement = false;
                }
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_x:
                        accel_fwd = true;
                        break;

                    case SDLK_z:
                        accel_bwd = true;
                        break;

                    case SDLK_q:
                        roll_left = true;
                        break;

                    case SDLK_e:
                        roll_right = true;
                        break;

                    case SDLK_w:
                        strafe_up = true;
                        break;

                    case SDLK_s:
                        strafe_down = true;
                        break;

                    case SDLK_a:
                        strafe_left = true;
                        break;

                    case SDLK_d:
                        strafe_right = true;
                        break;

                    case SDLK_ESCAPE:
                        quit_main_loop();
                        break;
                }
                break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_x:
                        accel_fwd = false;
                        break;

                    case SDLK_z:
                        accel_bwd = false;
                        break;

                    case SDLK_q:
                        roll_left = false;
                        break;

                    case SDLK_e:
                        roll_right = false;
                        break;

                    case SDLK_w:
                        strafe_up = false;
                        break;

                    case SDLK_s:
                        strafe_down = false;
                        break;

                    case SDLK_a:
                        strafe_left = false;
                        break;

                    case SDLK_d:
                        strafe_right = false;
                        break;
                }
                break;
        }
    }
}


void ui_swap_buffers(void)
{
    SDL_GL_SwapWindow(wnd);
}
