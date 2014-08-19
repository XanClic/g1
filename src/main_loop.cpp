#include <memory>

#include "graphics.hpp"
#include "main_loop.hpp"
#include "physics.hpp"
#include "ui.hpp"


static bool quit = false;


void main_loop(void)
{
    std::shared_ptr<WorldState> states[2] = { std::make_shared<WorldState>(), std::make_shared<WorldState>() };
    int source = 0;

    states[0]->initialize();

    while (!quit) {
        int next_source = (source + 1) % 2;

        ui_process_events(*states[next_source]);
        do_physics(*states[next_source], *states[source]);
        do_graphics(*states[source]);

        source = next_source;
    }
}


void quit_main_loop(void)
{
    quit = true;
}
