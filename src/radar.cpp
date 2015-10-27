#include <dake/math/fmatrix.hpp>

#include <cstdint>
#include <climits>
#include <vector>

#include "physics.hpp"
#include "radar.hpp"
#include "ship.hpp"
#include "ui.hpp"


using namespace dake::math;


void Radar::update(const Radar &radar_old, const ShipState &ship_new,
                   const WorldState &ws_new, const Input &user_input)
{
    selected = nullptr;
    selected_id = radar_old.selected_id;

    size_t oi = 0;
    for (const ShipState &ship: ws_new.ships) {
        if (&ship == &ship_new) {
            continue;
        }

        fvec3 rel_pos = ship.position - ship_new.position;

        // TODO: Check whether obstructed by earth
        if (rel_pos.length() >= 1e6f) {
            continue;
        }

        if (targets.size() <= oi) {
            targets.resize(oi + 1);
        }
        targets[oi].relative_position = rel_pos;

        // As a side note: We should *not* read anything but the position from
        // the WorldState, because in reality we'd be unable to obtain that
        // information directly (we could only get a scalar relative speed value
        // from the Doppler effect). But in order to do it correctly, we'd have
        // to do some magic for associating targets from the last iteration to
        // the ones seen now. That is both hard and computationally intensive,
        // with little actual gain. So we'll just assume we have some magic way
        // of identifying the target, and that's it.
        targets[oi].id = ship.id;

        // Oh, and getting the last position for actually calculating the
        // velocity as the difference is not trivial, either, as we'd have to
        // iterate through the old targets (or the full old ship list). Just
        // skip it, too.
        targets[oi].relative_velocity = ship.velocity - ship_new.velocity;

        if (ship.id == selected_id) {
            selected = &targets[oi];
        }

        oi++;
    }

    if (oi < targets.size()) {
        targets.resize(oi);
    }

    if (!selected) {
        selected_id = (uint64_t)-1;
    }

    if (&ship_new != &ws_new.ships[ws_new.player_ship]) {
        return;
    }

    if (&ship_new - ws_new.ships.data() == ws_new.player_ship) {
        if (user_input.get_mapping("next_target")) {
            if (targets.empty()) {
                selected = nullptr;
            } else if (!selected || selected == &targets.back()) {
                selected = &targets.front();
            } else {
                selected++;
            }

            if (selected) {
                selected_id = selected->id;
            } else {
                selected_id = (uint64_t)-1;
            }
        }

        if (user_input.get_mapping("previous_target")) {
            if (targets.empty()) {
                selected = nullptr;
            } else if (!selected || selected == &targets.front()) {
                selected = &targets.back();
            } else {
                selected--;
            }

            if (selected) {
                selected_id = selected->id;
            } else {
                selected_id = (uint64_t)-1;
            }
        }
    }
}
