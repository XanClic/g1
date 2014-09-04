#ifndef UI_HPP
#define UI_HPP

struct Input {
    float yaw, pitch, roll;
    float strafe_x, strafe_y, strafe_z;
    float main_engine;
};


void init_ui(void);

void ui_process_events(Input &input);
void ui_swap_buffers(void);

#endif
