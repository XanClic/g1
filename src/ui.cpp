#include <cmath>
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


enum MouseAxis {
    MOUSE_UNKNOWN,

    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_DOWN,
    MOUSE_UP,
};


enum GamepadAxis {
    AXIS_UNKNOWN,

    AXIS_L_LEFT,
    AXIS_L_RIGHT,
    AXIS_L_DOWN,
    AXIS_L_UP,
    AXIS_L_CW,
    AXIS_L_CCW,
    AXIS_R_LEFT,
    AXIS_R_RIGHT,
    AXIS_R_DOWN,
    AXIS_R_UP,
    AXIS_R_CW,
    AXIS_R_CCW,
    AXIS_ANALOG_LEFT,
    AXIS_ANALOG_RIGHT,
    AXIS_ANALOG_DOWN,
    AXIS_ANALOG_UP,
    AXIS_ANALOG_CW,
    AXIS_ANALOG_CCW,
    AXIS_LSHOULDER,
    AXIS_RSHOULDER,
};


namespace std
{
#define int_hash(type) \
    template<> struct hash<type> { \
        size_t operator()(const type &v) const { return hash<int>()(v); } \
    }

int_hash(SDL_Scancode);
int_hash(MouseAxis);
int_hash(SteamController::Button);
int_hash(GamepadAxis);

#undef int_hash
}


struct Action {
    enum Translate {
        NONE,

        // Button translations
        STICKY,
        ONE_SHOT,

        // Axis translations
        DIFFERENCE,
        CIRCLE_DIFFERENCE,
    };

    Action(void) {}
    Action(const std::string &n, Translate t = NONE):
        name(n), trans(t) {}

    Action &operator=(const std::string &n) { name = n; return *this; }

    std::string name;
    Translate trans = NONE;
    float multiplier = 1.f;

    bool registered = false, sticky_state = false;
    float previous_state = NAN;
};


static SDL_Window *wnd;
static SteamController *gamepad;
static int wnd_width, wnd_height;


static std::unordered_map<SDL_Scancode, Action> keyboard_mappings;
static std::unordered_map<int, Action> mouse_button_mappings;
static std::unordered_map<MouseAxis, Action> mouse_axis_mappings;
static std::unordered_map<SteamController::Button, Action>
    gamepad_button_mappings;
static std::unordered_map<GamepadAxis, Action> gamepad_axis_mappings;


static int get_mouse_button_from_name(const char *name)
{
    if (!strcmp(name, "LButton")) {
        return SDL_BUTTON_LEFT;
    } else if (!strcmp(name, "RButton")) {
        return SDL_BUTTON_RIGHT;
    } else if (!strcmp(name, "MButton")) {
        return SDL_BUTTON_MIDDLE;
    } else {
        return -1;
    }
}


static MouseAxis get_mouse_axis_from_name(const char *name)
{
    if (!strcmp(name, "Left")) {
        return MOUSE_LEFT;
    } else if (!strcmp(name, "Right")) {
        return MOUSE_RIGHT;
    } else if (!strcmp(name, "Down")) {
        return MOUSE_DOWN;
    } else if (!strcmp(name, "Up")) {
        return MOUSE_UP;
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
    } else if (!strcmp(name, "LeftPad.-z")) {
        return AXIS_L_CW;
    } else if (!strcmp(name, "LeftPad.+z")) {
        return AXIS_L_CCW;
    } else if (!strcmp(name, "RightPad.-x")) {
        return AXIS_R_LEFT;
    } else if (!strcmp(name, "RightPad.+x")) {
        return AXIS_R_RIGHT;
    } else if (!strcmp(name, "RightPad.-y")) {
        return AXIS_R_DOWN;
    } else if (!strcmp(name, "RightPad.+y")) {
        return AXIS_R_UP;
    } else if (!strcmp(name, "RightPad.-z")) {
        return AXIS_R_CW;
    } else if (!strcmp(name, "RightPad.+z")) {
        return AXIS_R_CCW;
    } else if (!strcmp(name, "Analog.-x")) {
        return AXIS_ANALOG_LEFT;
    } else if (!strcmp(name, "Analog.+x")) {
        return AXIS_ANALOG_RIGHT;
    } else if (!strcmp(name, "Analog.-y")) {
        return AXIS_ANALOG_DOWN;
    } else if (!strcmp(name, "Analog.+y")) {
        return AXIS_ANALOG_UP;
    } else if (!strcmp(name, "Analog.-z")) {
        return AXIS_ANALOG_CW;
    } else if (!strcmp(name, "Analog.+z")) {
        return AXIS_ANALOG_CCW;
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


static void verify_axis_target(const std::string &name, const Action &a)
{
    if (a.trans != Action::NONE && a.trans != Action::DIFFERENCE &&
        a.trans != Action::CIRCLE_DIFFERENCE)
    {
        throw std::runtime_error("Invalid translation for an axis event (for “"
                                 + name + "”");
    }
}


static void verify_button_target(const std::string &name, const Action &a)
{
    if (a.trans != Action::NONE && a.trans != Action::STICKY &&
        a.trans != Action::ONE_SHOT)
    {
        throw std::runtime_error("Invalid translation for an axis event (for “"
                                 + name + "”");
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
                } else if (trans_str == "difference") {
                    trans = Action::DIFFERENCE;
                } else if (trans_str == "circle_difference") {
                    trans = Action::CIRCLE_DIFFERENCE;
                } else {
                    throw std::runtime_error("Invalid value “" + trans_str +
                                             "” given as @translate for “" +
                                             m.first + "”");
                }
            }

            float multiplier = 1.f;
            auto multiplier_it = cm.find("multiplier");
            if (multiplier_it != cm.end()) {
                if (multiplier_it->second->type != GDData::FLOAT &&
                    multiplier_it->second->type != GDData::INTEGER)
                {
                    throw std::runtime_error("@multiplier value given for “" +
                                             m.first + "” not a float or "
                                             "integer");
                }

                if (multiplier_it->second->type == GDData::FLOAT) {
                    multiplier = (GDFloat)*multiplier_it->second;
                } else {
                    multiplier = (GDInteger)*multiplier_it->second;
                }
            }

            target.name = (const GDString &)*target_it->second;
            target.trans = trans;
            target.multiplier = multiplier;
        }

        if (!strncmp(m.first.c_str(), "Mouse.", 6)) {
            MouseAxis a = get_mouse_axis_from_name(m.first.c_str() + 6);
            if (a != MOUSE_UNKNOWN) {
                verify_axis_target(m.first, target);
                mouse_axis_mappings[a] = target;
            } else {
                int b = get_mouse_button_from_name(m.first.c_str() + 6);
                if (b >= 0) {
                    verify_button_target(m.first, target);
                    mouse_button_mappings[b] = target;
                } else {
                    throw std::runtime_error("Unknown mapping “" + m.first +
                                             "”");
                }
            }
        } else if (!strncmp(m.first.c_str(), "Gamepad.", 8)) {
            GamepadAxis a = get_gamepad_axis_from_name(m.first.c_str() + 8);
            if (a != AXIS_UNKNOWN) {
                verify_axis_target(m.first, target);
                gamepad_axis_mappings[a] = target;
            } else {
                SteamController::Button b =
                    get_gamepad_button_from_name(m.first.c_str() + 8);
                if (b != SteamController::NONE) {
                    verify_button_target(m.first, target);
                    gamepad_button_mappings[b] = target;
                } else {
                    throw std::runtime_error("Unknown mapping “" + m.first +
                                             "”");
                }
            }
        } else {
            SDL_Scancode sc = SDL_GetScancodeFromName(m.first.c_str());
            if (sc == SDL_SCANCODE_UNKNOWN) {
                throw std::runtime_error("Unknown mapping “" + m.first + "”");
            }
            verify_button_target(m.first, target);
            keyboard_mappings[sc] = target;
        }
    }
}


static float clamp(float x)
{
    return x < 0.f ? 0.f : x > 1.f ? 1.f : x;
}


static void button_down(Input &input, Action &action)
{
    float &s = input.mapping_states[action.name];

    if (action.trans == Action::STICKY) {
        if (!action.registered) {
            action.sticky_state = !action.sticky_state;
        }
        s = clamp(action.sticky_state ? action.multiplier : 0.f);
    } else if (action.trans == Action::ONE_SHOT) {
        s = clamp(action.registered ? 0.f : action.multiplier);
    } else {
        s = clamp(action.multiplier);
    }

    action.registered = true;
}


static void button_up(Input &input, Action &action)
{
    if (action.trans == Action::STICKY && action.sticky_state) {
        input.mapping_states[action.name] = clamp(action.multiplier);
    }

    action.registered = false;
}


static void update_1way_axis(Input &input, GamepadAxis axis, float state)
{
    auto it = gamepad_axis_mappings.find(axis);
    if (it != gamepad_axis_mappings.end()) {
        float &ns = input.mapping_states[it->second.name];
        if (it->second.trans == Action::DIFFERENCE) {
            float s = state;
            if (isnanf(it->second.previous_state)) {
                state = 0.f;
            } else {
                state -= it->second.previous_state;
            }
            it->second.previous_state = s;
        } else if (it->second.trans == Action::CIRCLE_DIFFERENCE) {
            // Has been taken care of already
        } else {
            // Respect dead zone (FIXME: This should be configurable)
            state = (state - .1f) / .9f;
        }

        ns = clamp(ns + state * it->second.multiplier);
    }
}


static void update_6way_axis(Input &input, GamepadAxis left, const vec2 &state,
                             vec2 &prev_state)
{
    vec2 difference;
    if (isnanf(prev_state.x())) {
        difference = vec2::zero();
    } else {
        difference = state - prev_state;
    }
    prev_state = state;

    float nrm = difference.length() * state.length();
    if (!nrm) {
        nrm = 1e-3f;
    }

    vec2 state_left_ortho(-state.y(), state.x());

    float straight_part = fabsf(difference.dot(state)) / nrm;
    float circle_part = difference.dot(state_left_ortho) / nrm;

    vec3 circle_difference;
    circle_difference.x() = straight_part * difference.x();
    circle_difference.y() = straight_part * difference.y();
    circle_difference.z() = circle_part * difference.length();

    for (int i = 0; i < 6; i++) {
        auto it = gamepad_axis_mappings.find(left);
        if (it != gamepad_axis_mappings.end()) {
            float value;

            if (it->second.trans == Action::CIRCLE_DIFFERENCE) {
                value = circle_difference[i / 2];
            } else {
                value = i < 4 ? state[i / 2] : 0.f;
            }

            if (!(i % 2)) {
                value = -value;
            }

            update_1way_axis(input, left, value);
        }

        left = static_cast<GamepadAxis>(left + 1);
    }
}


static void invalidate_6way_axis(GamepadAxis left, vec2 &prev_state)
{
    for (int i = 0; i < 6; i++) {
        auto it = gamepad_axis_mappings.find(left);
        if (it != gamepad_axis_mappings.end()) {
            it->second.previous_state = NAN;
        }

        left = static_cast<GamepadAxis>(left + 1);
    }

    prev_state.x() = NAN;
    prev_state.y() = NAN;
}


void process_gamepad_events(Input &input, SteamController *gp)
{
    static vec2 prev_lpad(NAN, NAN);
    static vec2 prev_rpad(NAN, NAN);
    static vec2 prev_analog(NAN, NAN);

    if (gp->lpad_valid()) {
        update_6way_axis(input, AXIS_L_LEFT, gp->lpad(), prev_lpad);
    } else {
        invalidate_6way_axis(AXIS_L_LEFT, prev_lpad);
    }
    if (gp->rpad_valid()) {
        update_6way_axis(input, AXIS_R_LEFT, gp->rpad(), prev_rpad);
    } else {
        invalidate_6way_axis(AXIS_R_LEFT, prev_rpad);
    }
    if (gp->analog_valid()) {
        update_6way_axis(input, AXIS_ANALOG_LEFT, gp->analog(), prev_analog);
    } else {
        invalidate_6way_axis(AXIS_ANALOG_LEFT, prev_analog);
    }

    update_1way_axis(input, AXIS_LSHOULDER, gp->lshoulder());
    update_1way_axis(input, AXIS_RSHOULDER, gp->rshoulder());


    for (auto &gbm: gamepad_button_mappings) {
        if (gp->button_state(gbm.first)) {
            button_down(input, gbm.second);
        } else {
            button_up(input, gbm.second);
        }
    }
}


static void update_mouse_axis(Input &input, MouseAxis axis, float state)
{
    auto it = mouse_axis_mappings.find(axis);
    if (it != mouse_axis_mappings.end()) {
        float &ns = input.mapping_states[it->second.name];
        if (it->second.trans == Action::DIFFERENCE) {
            float s = state;
            if (isnanf(it->second.previous_state)) {
                state = 0.f;
            } else {
                state -= it->second.previous_state;
            }
            it->second.previous_state = s;
        }

        ns = clamp(ns + state * it->second.multiplier);
    }
}


void ui_process_events(Input &input)
{
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
        }
    }


    for (auto &p: input.mapping_states) {
        p.second = 0.f;
    }

    int abs_x, abs_y;
    uint32_t mouse_state = SDL_GetMouseState(&abs_x, &abs_y);

    float rel_x = 2.f * abs_x / wnd_width - 1.f;
    float rel_y = 1.f - 2.f * abs_y / wnd_height;

    if (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        update_mouse_axis(input, MOUSE_LEFT,  -rel_x);
        update_mouse_axis(input, MOUSE_RIGHT,  rel_x);
        update_mouse_axis(input, MOUSE_DOWN,  -rel_y);
        update_mouse_axis(input, MOUSE_UP,     rel_y);
    }

    for (auto &mbm: mouse_button_mappings) {
        if (mouse_state & SDL_BUTTON(mbm.first)) {
            button_down(input, mbm.second);
        } else {
            button_up(input, mbm.second);
        }
    }


    const uint8_t *kbd_map = SDL_GetKeyboardState(nullptr);

    for (auto &km: keyboard_mappings) {
        if (kbd_map[km.first]) {
            button_down(input, km.second);
        } else {
            button_up(input, km.second);
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

    return it->second;
}
