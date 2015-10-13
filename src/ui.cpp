#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>

#include <dake/dake.hpp>
#include <SDL2/SDL.h>

#include "generic-data.hpp"
#include "graphics.hpp"
#include "json.hpp"
#include "localize.hpp"
#include "main_loop.hpp"
#include "physics.hpp"
#include "ui.hpp"


using namespace dake::math;


enum MouseEvent {
    MOUSE_UNKNOWN,

    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_DOWN,
    MOUSE_UP,

    MOUSE_RBUTTON,
};


namespace std
{
template<> struct hash<MouseEvent> {
    size_t operator()(const MouseEvent &d) const { return hash<int>()(d); }
};
}


struct Action {
    Action(void) {}
    Action(const std::string &n, bool s = false):
        name(n), sticky(s) {}

    Action &operator=(const std::string &n) { name = n; return *this; }

    std::string name;
    // No effects on mouse mappings!
    bool sticky = false;
};


static SDL_Window *wnd;
static int wnd_width, wnd_height;


static std::unordered_map<SDL_Keycode, Action> keyboard_mappings;
static std::unordered_map<MouseEvent, Action> mouse_mappings;


static MouseEvent get_mouse_event_from_name(const char *name)
{
    if (!strcmp(name, "left")) {
        return MOUSE_LEFT;
    } else if (!strcmp(name, "right")) {
        return MOUSE_RIGHT;
    } else if (!strcmp(name, "down")) {
        return MOUSE_DOWN;
    } else if (!strcmp(name, "up")) {
        return MOUSE_UP;
    } else if (!strcmp(name, "rbutton")) {
        return MOUSE_RBUTTON;
    } else {
        return MOUSE_UNKNOWN;
    }
}


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


    GDData *map_config = json_parse_file("config/input-bindings.json");
    if (map_config->type != GDData::OBJECT) {
        throw std::runtime_error("config/input-bindings.json must contain an "
                                 "object");
    }

    for (const auto &m: (const GDObject &)*map_config) {
        if (m.second->type != GDData::STRING &&
            m.second->type != GDData::OBJECT)
        {
            throw std::runtime_error("Input mapping target for “" + m.first +
                                     "” is not a string or object");
        }

        Action target;
        if (m.second->type == GDData::STRING) {
            target = (const GDString &)*m.second;
        } else {
            const GDObject &cm = *m.second;
            auto target_it = cm.find("target");
            if (target_it == cm.end() ||
                target_it->second->type != GDData::STRING)
            {
                throw std::runtime_error("No or non-string target specified "
                                         "for “" + m.first + "”");
            }

            bool sticky = false;
            auto sticky_it = cm.find("sticky");
            if (sticky_it != cm.end()) {
                if (sticky_it->second->type != GDData::BOOLEAN) {
                    throw std::runtime_error("@sticky value given for “" +
                                             m.first + "” not a boolean");
                }
                sticky = (GDBoolean)*sticky_it->second;
            }

            target.name = (const GDString &)*target_it->second;
            target.sticky = sticky;
        }

        if (!strncmp(m.first.c_str(), "mouse.", 6)) {
            MouseEvent me = get_mouse_event_from_name(m.first.c_str() + 6);
            if (me == MOUSE_UNKNOWN) {
                throw std::runtime_error("Unknown mapping “" + m.first + "”");
            }
            mouse_mappings[me] = target;
        } else {
            SDL_Keycode kc = SDL_GetKeyFromName(m.first.c_str());
            if (kc == SDLK_UNKNOWN) {
                throw std::runtime_error("Unknown mapping “" + m.first + "”");
            }
            keyboard_mappings[kc] = target;
        }
    }
}


void ui_process_events(Input &input)
{
    static bool capture_movement;

    if (capture_movement) {
        int abs_x, abs_y;
        SDL_GetMouseState(&abs_x, &abs_y);

        float rel_x = 2.f * abs_x / wnd_width - 1.f;
        float rel_y = 1.f - 2.f * abs_y / wnd_height;

        input.mapping_states[mouse_mappings[MOUSE_LEFT ].name] =
            dake::helper::maximum(-rel_x, 0.f);
        input.mapping_states[mouse_mappings[MOUSE_RIGHT].name] =
            dake::helper::maximum( rel_x, 0.f);
        input.mapping_states[mouse_mappings[MOUSE_DOWN ].name] =
            dake::helper::maximum(-rel_y, 0.f);
        input.mapping_states[mouse_mappings[MOUSE_UP   ].name] =
            dake::helper::maximum( rel_y, 0.f);
    } else {
        for (MouseEvent me: {MOUSE_LEFT, MOUSE_RIGHT, MOUSE_DOWN, MOUSE_UP}) {
            input.mapping_states[mouse_mappings[me].name] = 0.f;
        }
    }

    if (!input.initialized) {
        for (const auto &km: keyboard_mappings) {
            input.mapping_states[km.second.name] = 0.f;
        }
        input.initialized = true;
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

            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    capture_movement = true;
                    break;
                }

                MouseEvent evt = MOUSE_UNKNOWN;
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    evt = MOUSE_RBUTTON;
                }
                if (evt == MOUSE_UNKNOWN) {
                    break;
                }

                auto mapping = mouse_mappings.find(evt);
                if (mapping != mouse_mappings.end()) {
                    Input::MappingState &s =
                        input.mapping_states[mapping->second.name];

                    if (mapping->second.sticky) {
                        if (!s.registered) {
                            s.state = s.state ? 0.f : 1.f;
                        }
                        s.registered = true;
                    } else {
                        s = 1.f;
                    }
                }
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    capture_movement = false;
                    break;
                }

                MouseEvent evt = MOUSE_UNKNOWN;
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    evt = MOUSE_RBUTTON;
                }
                if (evt == MOUSE_UNKNOWN) {
                    break;
                }

                auto mapping = mouse_mappings.find(evt);
                if (mapping != mouse_mappings.end()) {
                    Input::MappingState &s =
                        input.mapping_states[mapping->second.name];

                    if (mapping->second.sticky) {
                        s.registered = false;
                    } else {
                        s = 0.f;
                    }
                }
                break;
            }

            case SDL_KEYDOWN: {
                auto mapping = keyboard_mappings.find(event.key.keysym.sym);
                if (mapping != keyboard_mappings.end()) {
                    Input::MappingState &s =
                        input.mapping_states[mapping->second.name];

                    if (mapping->second.sticky) {
                        if (!s.registered) {
                            s.state = s.state ? 0.f : 1.f;
                        }
                        s.registered = true;
                    } else {
                        s = 1.f;
                    }
                } else {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            quit_main_loop();
                            break;

                        case SDLK_F12:
                            olo = static_cast<Localization>((olo + 1) %
                                                            LOCALIZATIONS);
                    }
                }
                break;
            }

            case SDL_KEYUP: {
                auto mapping = keyboard_mappings.find(event.key.keysym.sym);
                if (mapping != keyboard_mappings.end()) {
                    Input::MappingState &s =
                        input.mapping_states[mapping->second.name];

                    if (mapping->second.sticky) {
                        s.registered = false;
                    } else {
                        s = 0.f;
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


float Input::get_mapping(const std::string &n) const
{
    auto it = mapping_states.find(n);
    if (it == mapping_states.end()) {
        return 0.f;
    }

    return it->second.state;
}
