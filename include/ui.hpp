#ifndef UI_HPP
#define UI_HPP

#include <string>
#include <unordered_map>

#include <dake/math/matrix.hpp>


struct Input {
    struct MappingState {
        MappingState &operator=(float f) { state = f; return *this; }
        operator float(void) const { return state; }

        float state;

        // For sticky keys, do not use outside of ui.cpp
        bool registered;
    };

    float get_mapping(const std::string &n) const
    { return mapping_states.find(n)->second; }

    bool initialized = false; // FIXME
    std::unordered_map<std::string, MappingState> mapping_states;
};


void init_ui(void);

void ui_process_events(Input &input);
void ui_process_menu_events(bool &quit, bool &mouse_down, dake::math::vec2 &mouse_pos);
void ui_swap_buffers(void);

#endif
