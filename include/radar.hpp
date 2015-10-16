#ifndef RADAR_HPP
#define RADAR_HPP

#include <dake/math/matrix.hpp>

#include <cstdint>
#include <vector>


struct WorldState;
struct ShipState;


struct RadarTarget {
    // Vector on the unit sphere is the direction the signal was sent to and
    // arrived from; the distance is determined by the travel time
    dake::math::vec3 relative_position;

    // The difference between the last relative position and the current
    // relative position (scaled by the time interval)
    dake::math::vec3 relative_velocity;

    uint64_t id;
};

class Radar {
    public:
        void update(const ShipState &ship_new, const WorldState &ws_new);

        std::vector<RadarTarget> targets;
};

#endif
