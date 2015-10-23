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
#include "ui.hpp"

#include "physics/PhysicsEngine.h"


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
        // TODO< avoid race condition >
        // transfer new physics state
        const float timeDelta = info.world_states[(info.current_physics_state + 1) % 2]->interval;
        info.world_states[info.current_physics_state]->physicsEngine->postStep(timeDelta);


        int next_state = (info.current_physics_state + 1) % 2;

        // TODO< UI has nothing lost inside physics >
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

    info.world_states[0]->physicsEngine = info.world_states[1]->physicsEngine = SharedPointer<PhysicsEngine>(new PhysicsEngine());

    info.world_states[0]->initialize(scenario);

    // Do one step, because some values are actually not initialized by
    // "initialize".
    ui_process_events(*info.input);

    do_physics(*info.world_states[1], *info.world_states[0], *info.input);
    // transfer new physics state
    const float timeDelta = info.world_states[(info.current_physics_state + 1) % 2]->interval;
    info.world_states[info.current_physics_state]->physicsEngine->postStep(timeDelta);

    info.current_graphics_state = 1;
    info.current_physics_state  = 1;

    std::thread physics_thr(physics_worker, std::ref(info));

    while (!quit) {
        do_graphics(*info.world_states[info.current_graphics_state]);

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
