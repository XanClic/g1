#ifndef AURORA_HPP
#define AURORA_HPP

#include <dake/math/matrix.hpp>

#include <vector>

class WorldState;


class Aurora {
    public:
        struct Hotspot {
            dake::math::vec2 center;
            float radius, strength;
        };

        class HotspotList {
            public:
                HotspotList(void);
                void step(const HotspotList &input, const WorldState &out_state);

                friend class Aurora;

            private:
                std::vector<Hotspot> hotspots;
        };

        Aurora(void);

        void step(const Aurora &input, const HotspotList &hotspots, const WorldState &out_state);

        const std::vector<dake::math::vec4> &samples(void) const
        { return spls; }


    private:
        struct CircularForce {
            dake::math::vec2 center;
            float radius, strength, duration, age;
        };

        std::vector<dake::math::vec4> spls;
        std::vector<dake::math::vec2> forces;
        std::vector<CircularForce> circulars;
};

#endif
