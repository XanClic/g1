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
#include "sound.hpp"
#include "ui.hpp"

#ifdef HAS_HIDAPI
#include "steam_controller.hpp"
#endif

#include <dake/cross/undef-macros.hpp>


using namespace dake::math;


enum MouseAxis {
    MOUSE_UNKNOWN,

    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_DOWN,
    MOUSE_UP,
};


#ifdef HAS_HIDAPI
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
#endif


namespace std
{
#define int_hash(type) \
    template<> struct hash<type> { \
        size_t operator()(const type &v) const { return hash<int>()(v); } \
    }

int_hash(SDL_Scancode);
int_hash(MouseAxis);

#ifdef HAS_HIDAPI
int_hash(SteamController::Button);
int_hash(GamepadAxis);
#endif

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
        FOV_ANGLE,
    };

    Action(void) {}
    Action(const std::string &n): name(n) {}

    Action &operator=(const std::string &n) { name = n; return *this; }

    std::string name;
    Translate trans = NONE;
    float multiplier = 1.f, deadzone = 0.f;
    bool is_modifier = false;

    bool registered = false, sticky_state = false;
    float previous_state = NAN;
    std::string only_if, only_unless;
};


static SDL_Window *wnd;
static int wnd_width, wnd_height;

#ifdef HAS_HIDAPI
static SteamController *gamepad;
#endif


static std::unordered_map<SDL_Scancode, std::vector<Action>> keyboard_mappings;
static std::unordered_map<int, std::vector<Action>> mouse_button_mappings;
static std::unordered_map<MouseAxis, std::vector<Action>> mouse_axis_mappings;

#ifdef HAS_HIDAPI
static std::unordered_map<SteamController::Button, std::vector<Action>>
    gamepad_button_mappings;
static std::unordered_map<GamepadAxis, std::vector<Action>>
    gamepad_axis_mappings;
#endif


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


#ifdef HAS_HIDAPI
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
    } else if (!strcmp(name, "Analog")) {
        return SteamController::ANALOG_STICK;
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
#endif


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


#ifdef HAS_HIDAPI
static void destroy_gamepad(void)
{
    if (gamepad) {
        delete gamepad;
        gamepad = nullptr;
    }
}
#endif


static void verify_axis_actions(const std::string &name,
                                const std::vector<Action> &al)
{
    for (const Action &a: al) {
        if (a.trans != Action::NONE && a.trans != Action::DIFFERENCE &&
            a.trans != Action::CIRCLE_DIFFERENCE &&
            a.trans != Action::FOV_ANGLE)
        {
            throw std::runtime_error("Invalid translation for an axis event "
                                     "(for “" + name + "”");
        }
    }
}


static void verify_button_actions(const std::string &name,
                                  const std::vector<Action> &al)
{
    for (const Action &a: al) {
        if (a.trans != Action::NONE && a.trans != Action::STICKY &&
            a.trans != Action::ONE_SHOT)
        {
            throw std::runtime_error("Invalid translation for an axis event "
                                     "(for “" + name + "”");
        }
    }
}


static void fill_action(std::vector<Action> *al, const GDString &cm)
{
    al->emplace_back(cm);

    if (!strncmp(cm.c_str(), "modifier.", 9)) {
        al->back().is_modifier = true;
    }
}


static void fill_action(std::vector<Action> *al, const GDObject &cm,
                        const std::string &event)
{
    al->emplace_back();
    Action &a = al->back();

    auto it = cm.find("target");
    if (it == cm.end() || it->second->type != GDData::STRING) {
        throw std::runtime_error("No or non-string target specified for “" +
                                 event + "”");
    }
    a.name = (const GDString &)*it->second;

    if (!strncmp(a.name.c_str(), "modifier.", 9)) {
        a.is_modifier = true;
    }

    it = cm.find("translate");
    if (it != cm.end()) {
        if (it->second->type != GDData::STRING) {
            throw std::runtime_error("@translate value given for “" +
                                     event + "” not a string");
        }

        const GDString &trans_str = *it->second;
        if (trans_str == "none") {
            a.trans = Action::NONE;
        } else if (trans_str == "sticky") {
            a.trans = Action::STICKY;
        } else if (trans_str == "one-shot") {
            a.trans = Action::ONE_SHOT;
        } else if (trans_str == "difference") {
            a.trans = Action::DIFFERENCE;
        } else if (trans_str == "circle_difference") {
            a.trans = Action::CIRCLE_DIFFERENCE;
        } else if (trans_str == "fov_angle") {
            a.trans = Action::FOV_ANGLE;
        } else {
            throw std::runtime_error("Invalid value “" + trans_str + "” given "
                                     "as @translate for “" + event + "”");
        }
    }

    it = cm.find("multiplier");
    if (it != cm.end()) {
        if (it->second->type != GDData::FLOAT &&
            it->second->type != GDData::INTEGER)
        {
            throw std::runtime_error("@multiplier value given for “" + event +
                                     "” not a float or integer");
        }

        if (it->second->type == GDData::FLOAT) {
            a.multiplier = (GDFloat)*it->second;
        } else {
            a.multiplier = (GDInteger)*it->second;
        }
    }

    it = cm.find("deadzone");
    if (it != cm.end()) {
        if (it->second->type != GDData::FLOAT) {
            throw std::runtime_error("@deadzone value given for “" + event +
                                     "” not a float");
        }

        a.deadzone = (GDFloat)*it->second;
    }

    it = cm.find("if");
    if (it != cm.end()) {
        if (it->second->type != GDData::STRING) {
            throw std::runtime_error("@if value given for “" + event +
                                     "” not a string");
        }

        a.only_if = (const GDString &)*it->second;
    }

    it = cm.find("unless");
    if (it != cm.end()) {
        if (it->second->type != GDData::STRING) {
            throw std::runtime_error("@unless value given for “" + event +
                                     "” not a string");
        }

        a.only_unless = (const GDString &)*it->second;
    }
}


static void fill_action_list(std::vector<Action> *al, const GDArray &cml,
                             const std::string &event)
{
    for (const GDData &element: cml) {
        if (element.type != GDData::STRING && element.type != GDData::OBJECT) {
            throw std::runtime_error("Input mapping list element given for “" +
                                     event + "” is not a string or object");
        }

        if (element.type == GDData::STRING) {
            fill_action(al, (const GDString &)element);
        } else {
            fill_action(al, (const GDObject &)element, event);
        }
    }
}


void init_ui(void)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

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


#ifdef HAS_HIDAPI
    try {
        gamepad = new SteamController;
        atexit(destroy_gamepad);
    } catch (std::exception &e) {
        fprintf(stderr, "Failed to initialize gamepad: %s\n", e.what());
    }
#endif


    init_graphics();
    set_resolution(1280, 720);

    wnd_width = 1280;
    wnd_height = 720;


    init_sound();


    GDData *map_config = json_parse_file("config/input-bindings.json");
    if (map_config->type != GDData::OBJECT) {
        throw std::runtime_error("config/input-bindings.json must contain an "
                                 "object");
    }

    for (const auto &m: (const GDObject &)*map_config) {
        if (m.second->type != GDData::STRING &&
            m.second->type != GDData::OBJECT &&
            m.second->type != GDData::ARRAY)
        {
            throw std::runtime_error("Input mapping target for “" + m.first +
                                     "” is not a string, object or array");
        }

        std::vector<Action> action_list;
        if (m.second->type == GDData::STRING) {
            fill_action(&action_list, (const GDString &)*m.second);
        } else if (m.second->type == GDData::OBJECT) {
            fill_action(&action_list, (const GDObject &)*m.second,
                        m.first);
        } else {
            fill_action_list(&action_list, (const GDArray &)*m.second, m.first);
        }

        if (!strncmp(m.first.c_str(), "Mouse.", 6)) {
            MouseAxis a = get_mouse_axis_from_name(m.first.c_str() + 6);
            if (a != MOUSE_UNKNOWN) {
                verify_axis_actions(m.first, action_list);
                mouse_axis_mappings[a] = action_list;
            } else {
                int b = get_mouse_button_from_name(m.first.c_str() + 6);
                if (b >= 0) {
                    verify_button_actions(m.first, action_list);
                    mouse_button_mappings[b] = action_list;
                } else {
                    throw std::runtime_error("Unknown mapping “" + m.first +
                                             "”");
                }
            }
        } else if (!strncmp(m.first.c_str(), "Gamepad.", 8)) {
#ifdef HAS_HIDAPI
            GamepadAxis a = get_gamepad_axis_from_name(m.first.c_str() + 8);
            if (a != AXIS_UNKNOWN) {
                verify_axis_actions(m.first, action_list);
                gamepad_axis_mappings[a] = action_list;
            } else {
                SteamController::Button b =
                    get_gamepad_button_from_name(m.first.c_str() + 8);
                if (b != SteamController::NONE) {
                    verify_button_actions(m.first, action_list);
                    gamepad_button_mappings[b] = action_list;
                } else {
                    throw std::runtime_error("Unknown mapping “" + m.first +
                                             "”");
                }
            }
#else
            fprintf(stderr, "Warning: Ignoring gamepad mapping %s; no gamepad "
                    "support compiled in\n", m.first.c_str());
#endif
        } else {
            SDL_Scancode sc = SDL_GetScancodeFromName(m.first.c_str());
            if (sc == SDL_SCANCODE_UNKNOWN) {
                throw std::runtime_error("Unknown mapping “" + m.first + "”");
            }
            verify_button_actions(m.first, action_list);
            keyboard_mappings[sc] = action_list;
        }
    }
}


static float clamp(float x)
{
    return x < 0.f ? 0.f : x > 1.f ? 1.f : x;
}


static bool modifiers_present(const Input &i, const Action &a)
{
    return (a.only_if.empty() || i.get_mapping(a.only_if)) &&
           (a.only_unless.empty() || !i.get_mapping(a.only_unless));
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


static void update_button(Input &input, Action &action, bool down)
{
    if (down && modifiers_present(input, action)) {
        button_down(input, action);
    } else {
        button_up(input, action);
    }
}


static void update_axis(Input &input, Action &a, float state)
{
    if (!modifiers_present(input, a)) {
        return;
    }

    float &ns = input.mapping_states[a.name];
    if (a.trans == Action::DIFFERENCE) {
        float s = state;
        if (isnanf(a.previous_state)) {
            state = 0.f;
        } else {
            state -= a.previous_state;
        }
        a.previous_state = s;
    } else if (a.trans == Action::CIRCLE_DIFFERENCE ||
               a.trans == Action::FOV_ANGLE)
    {
        // Has been taken care of already
    } else {
        state = (state - (a.deadzone)) / (1.f - a.deadzone);
    }

    ns = clamp(ns + state * a.multiplier);
}

#ifdef HAS_HIDAPI
static void update_1way_axis(Input &input, GamepadAxis axis, float state)
{
    if (state < 0.f) {
        state = 0.f;
    }

    auto it = gamepad_axis_mappings.find(axis);
    if (it != gamepad_axis_mappings.end()) {
        for (Action &a: it->second) {
            update_axis(input, a, state);
        }
    }
}


static void update_6way_axis(Input &input, GamepadAxis left, const fvec2 &state,
                             fvec2 &prev_state)
{
    fvec2 difference;
    if (isnanf(prev_state.x())) {
        difference = fvec2::zero();
    } else {
        difference = state - prev_state;
    }
    prev_state = state;

    float nrm = difference.length() * state.length();
    if (!nrm) {
        nrm = 1e-3f;
    }

    fvec2 state_left_ortho(-state.y(), state.x());

    float straight_part = fabsf(difference.dot(state)) / nrm;
    float circle_part = difference.dot(state_left_ortho) / nrm;

    fvec3 circle_difference;
    circle_difference.x() = straight_part * difference.x();
    circle_difference.y() = straight_part * difference.y();
    circle_difference.z() = circle_part * difference.length();

    for (int i = 0; i < 6; i++) {
        auto it = gamepad_axis_mappings.find(left);
        if (it != gamepad_axis_mappings.end()) {
            for (Action &a: it->second) {
                float value;

                if (a.trans == Action::CIRCLE_DIFFERENCE) {
                    value = circle_difference[i / 2];
                } else {
                    value = i < 4 ? state[i / 2] : 0.f;
                }

                if (!(i % 2)) {
                    value = -value;
                }

                update_axis(input, a, value);
            }
        }

        left = static_cast<GamepadAxis>(left + 1);
    }
}


static void invalidate_6way_axis(GamepadAxis left, fvec2 &prev_state)
{
    for (int i = 0; i < 6; i++) {
        auto it = gamepad_axis_mappings.find(left);
        if (it != gamepad_axis_mappings.end()) {
            for (Action &a: it->second) {
                a.previous_state = NAN;
            }
        }

        left = static_cast<GamepadAxis>(left + 1);
    }

    prev_state.x() = NAN;
    prev_state.y() = NAN;
}


void process_gamepad_events(Input &input, SteamController *gp, bool modifiers)
{
    static fvec2 prev_lpad(NAN, NAN);
    static fvec2 prev_rpad(NAN, NAN);
    static fvec2 prev_analog(NAN, NAN);

    if (!modifiers) {
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
            update_6way_axis(input, AXIS_ANALOG_LEFT, gp->analog(),
                             prev_analog);
        } else {
            invalidate_6way_axis(AXIS_ANALOG_LEFT, prev_analog);
        }

        update_1way_axis(input, AXIS_LSHOULDER, gp->lshoulder());
        update_1way_axis(input, AXIS_RSHOULDER, gp->rshoulder());
    }


    for (auto &gbm: gamepad_button_mappings) {
        for (Action &a: gbm.second) {
            if (modifiers == a.is_modifier) {
                update_button(input, a, gp->button_state(gbm.first));
            }
        }
    }
}
#endif


static void update_mouse_axis(Input &input, MouseAxis axis, float state, bool y)
{
    if (state < 0.f) {
        state = 0.f;
    }

    auto it = mouse_axis_mappings.find(axis);
    if (it != mouse_axis_mappings.end()) {
        for (Action &a: it->second) {
            if (a.trans == Action::FOV_ANGLE) {
                // FIXME: We don't know that!
                const float yfov = M_PIf / 3.f;

                if (y) {
                    state *= tanf(.5f * yfov);
                } else {
                    state *= tanf(.5f * yfov) * wnd_width / wnd_height;
                }
            }

            update_axis(input, a, state);
        }
    }
}


static void process_mouse_events(Input &input, bool modifiers)
{
    int abs_x, abs_y;
    uint32_t mouse_state = SDL_GetMouseState(&abs_x, &abs_y);

    if (!modifiers) {
        float rel_x = 2.f * abs_x / wnd_width - 1.f;
        float rel_y = 1.f - 2.f * abs_y / wnd_height;

        update_mouse_axis(input, MOUSE_LEFT,  -rel_x, false);
        update_mouse_axis(input, MOUSE_RIGHT,  rel_x, false);
        update_mouse_axis(input, MOUSE_DOWN,  -rel_y, true);
        update_mouse_axis(input, MOUSE_UP,     rel_y, true);
    }

    for (auto &mbm: mouse_button_mappings) {
        for (Action &a: mbm.second) {
            if (modifiers == a.is_modifier) {
                update_button(input, a, mouse_state & SDL_BUTTON(mbm.first));
            }
        }
    }
}


static void process_keyboard_events(Input &input, bool modifiers)
{
    static const uint8_t *kbd_map;
    if (!kbd_map) {
        kbd_map = SDL_GetKeyboardState(nullptr);
    }

    for (auto &km: keyboard_mappings) {
        for (Action &a: km.second) {
            if (modifiers == a.is_modifier) {
                update_button(input, a, kbd_map[km.first]);
            }
        }
    }
}


void ui_process_events(Input &input)
{
    SDL_Event event;

    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT,
                          SDL_FIRSTEVENT, SDL_LASTEVENT) > 0)
    {
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

    process_mouse_events(input, true);
    process_keyboard_events(input, true);
#ifdef HAS_HIDAPI
    if (gamepad) {
        process_gamepad_events(input, gamepad, true);
    }
#endif

    process_mouse_events(input, false);
    process_keyboard_events(input, false);
#ifdef HAS_HIDAPI
    if (gamepad) {
        process_gamepad_events(input, gamepad, false);
    }
#endif


    if (input.get_mapping("quit")) {
        quit_main_loop();
    }
    if (input.get_mapping("next_localization")) {
        olo = static_cast<Localization>((olo + 1) % LOCALIZATIONS);
    }
}


void ui_fetch_events(Input &input)
{
    (void)input;

    SDL_PumpEvents();
}


void ui_process_menu_events(bool &quit, bool &mouse_down,
                            dake::math::fvec2 &mouse_pos)
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


void do_force_feedback(const WorldState &ws)
{
#ifdef HAS_HIDAPI
    if (!gamepad) {
        return;
    }

    // FIXME: We need user configuration for this (when to rumble, how to
    //        rumble, ...)

    const ShipState &ps = ws.ships[ws.player_ship];

    float total_thrust = 0.f;
    int thruster_count = ps.ship->thrusters.size();
    for (int i = 0; i < thruster_count; i++) {
        total_thrust += clamp(ps.thruster_states[i]) *
                        ps.ship->thrusters[i].force.length();
    }

    // FIXME: And this multiplier is completely arbitrary
    gamepad->set_right_rumble(clamp(.05f * total_thrust / ps.total_mass));


    for (bool fired: ps.weapon_fired) {
        if (fired) {
            gamepad->set_left_rumble(1.f, true);
            break;
        }
    }
#endif
}
