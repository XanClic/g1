#ifndef AURORA_HPP
#define AURORA_HPP

#include <dake/math/matrix.hpp>

#include <vector>

class WorldState;


class Aurora {
    public:
        Aurora(void);

        void step(const Aurora &input, const WorldState &out_state);

        const std::vector<dake::math::vec2> &samples(void) const
        { return spls; }


    private:
        struct CircularForce {
            dake::math::vec2 center;
            float radius, strength, duration, age;
        };

        std::vector<dake::math::vec2> spls, forces;
        std::vector<CircularForce> circulars;
};

#endif
