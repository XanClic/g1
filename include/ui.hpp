#ifndef UI_HPP
#define UI_HPP

#include <dake/math/matrix.hpp>


struct Input {
    float yaw, pitch, roll;
    float strafe_x, strafe_y, strafe_z;
    float main_engine;
};


void init_ui(void);

void ui_process_events(Input &input);
void ui_process_menu_events(bool &quit, bool &mouse_down, dake::math::vec2 &mouse_pos);
void ui_swap_buffers(void);

#endif
