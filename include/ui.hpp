#ifndef UI_HPP
#define UI_HPP

#include <string>
#include <unordered_map>

#include <dake/math/fmatrix.hpp>


struct WorldState;


struct Input {
    float get_mapping(const std::string &n) const;

    std::unordered_map<std::string, float> mapping_states;
};


void init_ui(void);

void ui_fetch_events(Input &input);
void ui_process_events(Input &input);
void ui_process_menu_events(bool &quit, bool &mouse_down,
                            dake::math::fvec2 &mouse_pos);

void do_force_feedback(const WorldState &ws);

void ui_swap_buffers(void);

#endif
