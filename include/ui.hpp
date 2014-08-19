#ifndef UI_HPP
#define UI_HPP

#include "physics.hpp"


void init_ui(void);

void ui_process_events(WorldState &state);
void ui_swap_buffers(void);

#endif
