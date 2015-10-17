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
#include "steam_controller.hpp"
#include "ui.hpp"


using namespace dake::math;


enum MouseEvent {
    MOUSE_UNKNOWN,

    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_DOWN,
    MOUSE_UP,

    MOUSE_RBUTTON,
    MOUSE_MBUTTON,
};


enum GamepadAxis {
    AXIS_UNKNOWN,

    AXIS_L_LEFT,
    AXIS_L_RIGHT,
    AXIS_L_DOWN,
    AXIS_L_UP,
    AXIS_R_LEFT,
    AXIS_R_RIGHT,
    AXIS_R_DOWN,
    AXIS_R_UP,
    AXIS_ANALOG_LEFT,
    AXIS_ANALOG_RIGHT,
    AXIS_ANALOG_DOWN,
    AXIS_ANALOG_UP,
    AXIS_LSHOULDER,
    AXIS_RSHOULDER,
};


namespace std
{
template<> struct hash<MouseEvent> {
    size_t operator()(const MouseEvent &d) const { return hash<int>()(d); }
};

template<> struct hash<SteamController::Button> {
    size_t operator()(const SteamController::Button &b) const { return hash<int>()(b); }
};

template<> struct hash<GamepadAxis> {
    size_t operator()(const GamepadAxis &a) const { return hash<int>()(a); }
};
}


struct Action {
    enum Translate {
        NONE,
        STICKY,
        ONE_SHOT,
    };

    Action(void) {}
    Action(const std::string &n, Translate t = NONE):
        name(n), trans(t) {}

    Action &operator=(const std::string &n) { name = n; return *this; }

    std::string name;
    // No effects on mouse mappings!
    Translate trans = NONE;
};


static SDL_Window *wnd;
static SteamController *gamepad;
static int wnd_width, wnd_height;


static std::unordered_map<SDL_Keycode, Action> keyboard_mappings;
static std::unordered_map<MouseEvent, Action> mouse_mappings;
static std::unordered_map<SteamController::Button, Action>
    gamepad_button_mappings;
static std::unordered_map<GamepadAxis, Action> gamepad_axis_mappings;


static MouseEvent get_mouse_event_from_name(const char *name)
{
    if (!strcmp(name, "Left")) {
        return MOUSE_LEFT;
    } else if (!strcmp(name, "Right")) {
        return MOUSE_RIGHT;
    } else if (!strcmp(name, "Down")) {
        return MOUSE_DOWN;
    } else if (!strcmp(name, "Up")) {
        return MOUSE_UP;
    } else if (!strcmp(name, "RButton")) {
        return MOUSE_RBUTTON;
    } else if (!strcmp(name, "MButton")) {
        return MOUSE_MBUTTON;
    } else {
        return MOUSE_UNKNOWN;
    }
}


static SteamController::Button get_gamepad_button_from_name(const char *name)
{
    if (!strcmp(name, "BottomLeftShoulder")) {
        return SteamController::BOT_LSHOULDER;
    } else if (!strcmp(name, "BottomRightShoulder")) {
        return SteamController::BOT_RSHOULDER;
    } else if (!strcmp(name, "TopLeftShoulder")) {
        return SteamController::TOP_LSHOULDER;
    } else if (!strcmp(name, "TopRightShoulder")) {
        return SteamController::TOP_RSHOULDER;
    } else if (!strcmp(name, "Y")) {
        return SteamController::Y;
    } else if (!strcmp(name, "B")) {
        return SteamController::B;
    } else if (!strcmp(name, "X")) {
        return SteamController::X;
    } else if (!strcmp(name, "A")) {
        return SteamController::A;
    } else if (!strcmp(name, "Up")) {
        return SteamController::UP;
    } else if (!strcmp(name, "Right")) {
        return SteamController::RIGHT;
    } else if (!strcmp(name, "Left")) {
        return SteamController::LEFT;
    } else if (!strcmp(name, "Down")) {
        return SteamController::DOWN;
    } else if (!strcmp(name, "Previous")) {
        return SteamController::PREVIOUS;
    } else if (!strcmp(name, "Action")) {
        return SteamController::ACTION;
    } else if (!strcmp(name, "Next")) {
        return SteamController::NEXT;
    } else if (!strcmp(name, "LeftGrip")) {
        return SteamController::LGRIP;
    } else if (!strcmp(name, "RightGrip")) {
        return SteamController::RGRIP;
    } else if (!strcmp(name, "LeftPad")) {
        return SteamController::LEFT_PAD;
    } else if (!strcmp(name, "RightPad")) {
        return SteamController::RIGHT_PAD;
    } else {
        return SteamController::NONE;
    }
}


static GamepadAxis get_gamepad_axis_from_name(const char *name)
{
    if (!strcmp(name, "LeftPad.-x")) {
        return AXIS_L_LEFT;
    } else if (!strcmp(name, "LeftPad.+x")) {
        return AXIS_L_RIGHT;
    } else if (!strcmp(name, "LeftPad.-y")) {
        return AXIS_L_DOWN;
    } else if (!strcmp(name, "LeftPad.+y")) {
        return AXIS_L_UP;
    } else if (!strcmp(name, "RightPad.-x")) {
        return AXIS_R_LEFT;
    } else if (!strcmp(name, "RightPad.+x")) {
        return AXIS_R_RIGHT;
    } else if (!strcmp(name, "RightPad.-y")) {
        return AXIS_R_DOWN;
    } else if (!strcmp(name, "RightPad.+y")) {
        return AXIS_R_UP;
    } else if (!strcmp(name, "Analog.-x")) {
        return AXIS_ANALOG_LEFT;
    } else if (!strcmp(name, "Analog.+x")) {
        return AXIS_ANALOG_RIGHT;
    } else if (!strcmp(name, "Analog.-y")) {
        return AXIS_ANALOG_DOWN;
    } else if (!strcmp(name, "Analog.+y")) {
        return AXIS_ANALOG_UP;
    } else if (!strcmp(name, "BottomLeftShoulder.+z")) {
        return AXIS_LSHOULDER;
    } else if (!strcmp(name, "BottomRightShoulder.+z")) {
        return AXIS_RSHOULDER;
    } else {
        return AXIS_UNKNOWN;
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


static void destroy_gamepad(void)
{
    if (gamepad) {
        delete gamepad;
        gamepad = nullptr;
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


    try {
        gamepad = new SteamController;
        atexit(destroy_gamepad);
    } catch (std::exception &e) {
        fprintf(stderr, "Failed to initialize gamepad: %s\n", e.what());
    }


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

            Action::Translate trans = Action::NONE;
            auto trans_it = cm.find("translate");
            if (trans_it != cm.end()) {
                if (trans_it->second->type != GDData::STRING) {
                    throw std::runtime_error("@translate value given for “" +
                                             m.first + "” not a string");
                }

                const GDString &trans_str = *trans_it->second;
                if (trans_str == "none") {
                    trans = Action::NONE;
                } else if (trans_str == "sticky") {
                    trans = Action::STICKY;
                } else if (trans_str == "one-shot") {
                    trans = Action::ONE_SHOT;
                } else {
                    throw std::runtime_error("Invalid value “" + trans_str +
                                             "” given as @translate for “" +
                                             m.first + "”");
                }
            }

            target.name = (const GDString &)*target_it->second;
            target.trans = trans;
        }

        if (!strncmp(m.first.c_str(), "Mouse.", 6)) {
            MouseEvent me = get_mouse_event_from_name(m.first.c_str() + 6);
            if (me == MOUSE_UNKNOWN) {
                throw std::runtime_error("Unknown mapping “" + m.first + "”");
            }
            mouse_mappings[me] = target;
        } else if (!strncmp(m.first.c_str(), "Gamepad.", 8)) {
            GamepadAxis a = get_gamepad_axis_from_name(m.first.c_str() + 8);
            if (a != AXIS_UNKNOWN) {
                gamepad_axis_mappings[a] = target;
            } else {
                SteamController::Button b =
                    get_gamepad_button_from_name(m.first.c_str() + 8);
                if (b != SteamController::NONE) {
                    gamepad_button_mappings[b] = target;
                } else {
                    throw std::runtime_error("Unknown mapping “" + m.first +
                                             "”");
                }
            }
        } else {
            SDL_Keycode kc = SDL_GetKeyFromName(m.first.c_str());
            if (kc == SDLK_UNKNOWN) {
                throw std::runtime_error("Unknown mapping “" + m.first + "”");
            }
            keyboard_mappings[kc] = target;
        }
    }
}


static void button_down(Input &input, const Action &action)
{
    Input::MappingState &s = input.mapping_states[action.name];

    if (action.trans == Action::STICKY) {
        if (!s.registered) {
            s.state = s.state ? 0.f : 1.f;
        }
    } else if (action.trans == Action::ONE_SHOT) {
        s.state = s.registered ? 0.f : 1.f;
    } else {
        s = 1.f;
    }
    s.registered = true;
}


static void button_up(Input &input, const Action &action)
{
    Input::MappingState &s = input.mapping_states[action.name];

    s.registered = false;
    if (action.trans != Action::STICKY) {
        s = 0.f;
    }
}


static float clamp(float x)
{
    return x < 0.f ? 0.f : x > 1.f ? 1.f : x;
}


static void update_1way_axis(Input &input, GamepadAxis axis, float state)
{
    auto it = gamepad_axis_mappings.find(axis);
    if (it != gamepad_axis_mappings.end()) {
        float &ns = input.mapping_states[it->second.name].state;
        // Respect dead zone (FIXME: This should be configurable)
        ns = clamp(ns + (state - .1f) / .9f);
    }
}


static void update_4way_axis(Input &input, GamepadAxis left, const vec2 &state)
{
    for (int i = 0; i < 4; i++) {
        float value = state[i / 2];
        if (!(i % 2)) {
            value = -value;
        }

        update_1way_axis(input, left, value);

        left = static_cast<GamepadAxis>(left + 1);
    }
}


void process_gamepad_events(Input &input, SteamController *gp)
{
    if (gp->lpad_valid()) {
        update_4way_axis(input, AXIS_L_LEFT, gp->lpad());
    }
    if (gp->rpad_valid()) {
        update_4way_axis(input, AXIS_R_LEFT, gp->rpad());
    }
    if (gp->analog_valid()) {
        update_4way_axis(input, AXIS_ANALOG_LEFT, gp->analog());
    }

    update_1way_axis(input, AXIS_LSHOULDER, gp->lshoulder());
    update_1way_axis(input, AXIS_RSHOULDER, gp->rshoulder());


    SteamController::Button button;
    while ((button = gp->next_button_down()) != SteamController::NONE) {
        auto it = gamepad_button_mappings.find(button);
        if (it != gamepad_button_mappings.end()) {
            button_down(input, it->second);
        }
    }

    while ((button = gp->next_button_up()) != SteamController::NONE) {
        auto it = gamepad_button_mappings.find(button);
        if (it != gamepad_button_mappings.end()) {
            button_up(input, it->second);
        }
    }
}


static void update_mouse_axis(Input &input, MouseEvent evt, float state)
{
    auto it = mouse_mappings.find(evt);
    if (it != mouse_mappings.end()) {
        input.mapping_states[it->second.name] = clamp(state);
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

        update_mouse_axis(input, MOUSE_LEFT,  -rel_x);
        update_mouse_axis(input, MOUSE_RIGHT,  rel_x);
        update_mouse_axis(input, MOUSE_DOWN,  -rel_y);
        update_mouse_axis(input, MOUSE_UP,     rel_y);
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

    for (const auto &km: keyboard_mappings) {
        if (km.second.trans == Action::ONE_SHOT) {
            input.mapping_states[km.second.name] = 0.f;
        }
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
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    evt = MOUSE_MBUTTON;
                }

                auto mapping = mouse_mappings.find(evt);
                if (mapping != mouse_mappings.end()) {
                    button_down(input, mapping->second);
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
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    evt = MOUSE_MBUTTON;
                }

                auto mapping = mouse_mappings.find(evt);
                if (mapping != mouse_mappings.end()) {
                    button_up(input, mapping->second);
                }
                break;
            }

            case SDL_KEYDOWN: {
                auto mapping = keyboard_mappings.find(event.key.keysym.sym);
                if (mapping != keyboard_mappings.end()) {
                    button_down(input, mapping->second);
                }
                break;
            }

            case SDL_KEYUP: {
                auto mapping = keyboard_mappings.find(event.key.keysym.sym);
                if (mapping != keyboard_mappings.end()) {
                    button_up(input, mapping->second);
                }
                break;
            }
        }
    }


    if (gamepad) {
        process_gamepad_events(input, gamepad);
    }


    if (input.get_mapping("quit")) {
        quit_main_loop();
    }
    if (input.get_mapping("next_localization")) {
        olo = static_cast<Localization>((olo + 1) % LOCALIZATIONS);
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
