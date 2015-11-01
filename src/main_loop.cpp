#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "graphics.hpp"
#include "main_loop.hpp"
#include "physics.hpp"
#include "sound.hpp"
#include "ui.hpp"


static bool quit = false;


struct SharedInfo {
    std::vector<std::shared_ptr<WorldState>> world_states;
    std::shared_ptr<Input> input;
    volatile int current_graphics_state, current_physics_state;
    std::condition_variable cgs_change;
    std::mutex cgs_change_mtx;
};


static void physics_worker(SharedInfo &info)
{
    std::unique_lock<std::mutex> lock(info.cgs_change_mtx);

    assert(info.world_states.size() == 2);

    while (!quit) {
        while (info.current_physics_state != info.current_graphics_state) {
            info.cgs_change.wait(lock);
        }

        int next_state = (info.current_physics_state + 1) % 2;

        ui_process_events(*info.input);
        do_physics(*info.world_states[next_state], *info.world_states[info.current_physics_state], *info.input);

        info.current_physics_state = next_state;
    }
}


void main_loop(const std::string &scenario)
{
    SharedInfo info;

    std::unique_lock<std::mutex> lock(info.cgs_change_mtx);
    lock.unlock();

    info.current_graphics_state = 0;
    info.current_physics_state  = 0;

    info.input = std::make_shared<Input>();
    info.world_states.emplace_back(new WorldState);
    info.world_states.emplace_back(new WorldState);

    info.world_states[0]->initialize(scenario);

    // Do one step, because some values are actually not initialized by
    // "initialize".
    ui_process_events(*info.input);
    do_physics(*info.world_states[1], *info.world_states[0], *info.input);
    info.current_graphics_state = 1;
    info.current_physics_state  = 1;

    std::thread physics_thr(physics_worker, std::ref(info));

    while (!quit) {
        do_graphics(*info.world_states[info.current_graphics_state]);
        do_force_feedback(*info.world_states[info.current_graphics_state]);
        do_sound(*info.world_states[info.current_graphics_state]);

        lock.lock();

        info.current_graphics_state = (info.current_graphics_state + 1) % 2;
        info.cgs_change.notify_all();

        lock.unlock();
    }

    physics_thr.join();
}


void quit_main_loop(void)
{
    quit = true;
}
