#ifndef AURORA_HPP
#define AURORA_HPP

#include <dake/math/matrix.hpp>
#include <dake/math/fmatrix.hpp>

#include <vector>

#include "align-allocator.hpp"

struct WorldState;


class Aurora {
    public:
        struct Hotspot {
            dake::math::fvec2 center;
            float radius, strength;
        };

        class HotspotList {
            public:
                HotspotList(void);
                void step(const HotspotList &input,
                          const WorldState &out_state);

                friend class Aurora;

            private:
                AlignedVector<Hotspot> hotspots;
        };

        struct Sample {
            dake::math::vec2 position;
            float texcoord, strength;
        };

        Aurora(void);

        void step(const Aurora &input, const HotspotList &hotspots,
                  const WorldState &out_state);

        const AlignedVector<Sample> &samples(void) const { return spls; }


    private:
        struct CircularForce {
            dake::math::fvec2 center;
            float radius, strength, duration, age;
        };

        AlignedVector<Sample> spls;
        AlignedVector<dake::math::fvec2> forces;
        AlignedVector<CircularForce> circulars;
};

#endif
