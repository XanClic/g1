#ifndef UI_HPP
#define UI_HPP

#include <string>
#include <unordered_map>

#include <dake/math/matrix.hpp>


struct Input {
    bool initialized = false; // FIXME
    std::unordered_map<std::string, float> mapping_states;
};


void init_ui(void);

void ui_process_events(Input &input);
void ui_process_menu_events(bool &quit, bool &mouse_down, dake::math::vec2 &mouse_pos);
void ui_swap_buffers(void);

#endif
