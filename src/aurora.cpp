#include <dake/dake.hpp>

#include <random>

#include "aurora.hpp"
#include "physics.hpp"


using namespace dake;
using namespace dake::math;


Aurora::Aurora(void)
{
    spls.resize(128);
    forces.resize(128);

    std::default_random_engine rng((uintptr_t)this);
    std::uniform_real_distribution<float> rng_dist(0.f, 1.f);

    for (size_t i = 0; i < spls.size(); i++) {
        spls[i].x() = 2.f * M_PIf * i / spls.size();
        spls[i].y() = (2.5f * rng_dist(rng) + 17.5f) / 180.f * M_PIf;
        spls[i].z() = 0.f;
        spls[i].w() = 0.f;
    }
}


static float smallest_angle(float x)
{
    x = fmodf(x, 2.f * M_PIf);
    return (x >  M_PIf) ? x - 2.f * M_PIf
         : (x < -M_PIf) ? x + 2.f * M_PIf
                        : x;
}


void Aurora::step(const Aurora &input, const HotspotList &hotspots, const WorldState &out_state)
{
    memset(forces.data(), 0, forces.size() * sizeof(vec2));

    std::default_random_engine rng(out_state.timestamp.time_since_epoch().count() + (uintptr_t)this);
    std::uniform_real_distribution<float> rng_dist(0.f, 1.f);

    circulars = input.circulars;

    if (rng_dist(rng) >= 1.f - out_state.interval / 10.f) {
        CircularForce cf = {
            vec2(rng_dist(rng) * 2.f * M_PIf,
                 (10.f + rng_dist(rng) * 15.f) / 180.f * M_PIf),
            rng_dist(rng) * .3f + .3f,
            rng_dist(rng) * 2.f - 1.f,
            rng_dist(rng) * 180.f,
            0.f
        };

        circulars.push_back(cf);
    }

    for (auto it = circulars.begin(); it != circulars.end();) {
        it->age += out_state.interval;

        if (it->age > it->duration) {
            it = circulars.erase(it);
        } else {
            ++it;
        }
    }

    for (const CircularForce &cf: input.circulars) {
        float real_strength = cf.strength * powf(-4.f * (cf.age / cf.duration - .5f) + 1.f, 2.f);

        for (size_t i = 0; i < input.spls.size(); i++) {
            vec2 direction = vec2(input.spls[i]) - cf.center;

            direction.x() = smallest_angle(direction.x());
            direction.y() = smallest_angle(direction.y());

            // FIXME: Use meters as radius unit
            float rel_dist = direction.length() / cf.radius;

            if (rel_dist > 1.f) {
                continue;
            }

            float this_strength = .01f * real_strength * rel_dist * expf(-4.f * rel_dist);

            forces[i] += this_strength * vec2(-direction.y(), direction.x()).normalized();
        }
    }

    for (size_t i = 0; i < input.spls.size(); i++) {
        const vec2 &this_sample = input.spls[i];
        const vec2 &next_sample = input.spls[(i + input.spls.size() - 1) % input.spls.size()];
        const vec2 &prev_sample = input.spls[(i                     + 1) % input.spls.size()];

        forces[i] += .02f * vec2(smallest_angle(next_sample.x() - this_sample.x()), next_sample.y() - this_sample.y());
        forces[i] += .02f * vec2(smallest_angle(prev_sample.x() - this_sample.x()), prev_sample.y() - this_sample.y());

        forces[i] += .02f * vec2(0.f, 17.5f / 180.f * M_PIf - input.spls[i].y());
    }

    float texcoord = 0.f;

    for (size_t i = 0; i < input.spls.size(); i++) {
        spls[i] = vec2(input.spls[i]) + out_state.interval * forces[i];

        if (i > 0) {
            texcoord += (vec2(input.spls[i - 1]) - vec2(input.spls[i])).length() * 2.f;
            texcoord = fmodf(texcoord, 1.f);
        }
        spls[i].z() = texcoord;
    }

    for (size_t i = 0; i < hotspots.hotspots.size(); i++) {
        const Hotspot &hs = hotspots.hotspots[i];

        for (vec4 &s: spls) {
            vec2 direction = vec2(s) - hs.center;

            direction.x() = smallest_angle(direction.x());
            direction.y() = smallest_angle(direction.y());

            float rel_dist = direction.length() / hs.radius;
            if (rel_dist > 1.f) {
                continue;
            }

            s.w() += hs.strength * powf(1.f - rel_dist, 2.f) * 5.f;
        }
    }
}


Aurora::HotspotList::HotspotList(void)
{
    std::default_random_engine rng((uintptr_t)this);
    std::uniform_real_distribution<float> rng_dist(0.f, 1.f);

    hotspots.resize(32);

    for (Hotspot &hs: hotspots) {
        hs.center   = vec2(rng_dist(rng) * 2.f * M_PIf,
                           (10.f + rng_dist(rng) * 15.f) / 180.f * M_PIf);
        hs.radius   = rng_dist(rng) * .2f + .05f;
        hs.strength = .05f / hs.radius;
    }
}


void Aurora::HotspotList::step(const HotspotList &input, const WorldState &out_state)
{
    std::default_random_engine rng(out_state.timestamp.time_since_epoch().count() + (uintptr_t)this);
    std::uniform_real_distribution<float> rng_dist(0.f, 1.f);

    for (size_t i = 0; i < input.hotspots.size(); i++) {
        hotspots[i] = input.hotspots[i];
        hotspots[i].center.x() = fmodf(input.hotspots[i].center.x() + .1f * out_state.interval * (rng_dist(rng) - .5f), 2.f * M_PIf);
    }
}
