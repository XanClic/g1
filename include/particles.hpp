#ifndef PARTICLES_HPP
#define PARTICLES_HPP

#include <dake/math/matrix.hpp>
#include <dake/math/fmatrix.hpp>

#include <vector>

#include "align-allocator.hpp"


struct ParticleGraphicsData {
    dake::math::vec3 position_relative_to_viewer;
    dake::math::vec3 orientation;
};

struct ParticleNonGraphicsData {
    dake::math::fvec3d position;
    dake::math::fvec3 velocity;
    float lifetime;
    uint64_t source_ship_id;
};

struct ImpactGraphicsData {
    dake::math::vec3 position_relative_to_viewer;
    float lifetime, total_lifetime;
};

struct ImpactNonGraphicsData {
    dake::math::fvec3d position;
    dake::math::fvec3 velocity;
};


struct Particles {
    AlignedVector<ParticleGraphicsData> pgd;
    AlignedVector<ParticleNonGraphicsData> pngd;

    AlignedVector<ImpactGraphicsData> igd;
    AlignedVector<ImpactNonGraphicsData> ingd;
};


struct WorldState;
struct GraphicsStatus;
struct ShipState;


void init_particles(void);

void spawn_particle(WorldState &output, const ShipState &sender,
                    const dake::math::fvec3d &position,
                    const dake::math::fvec3 &velocity,
                    const dake::math::fvec3 &orientation);

void handle_particles(Particles &output, const Particles &input,
                      WorldState &out_ws, const ShipState &player);

void draw_particles(const GraphicsStatus &state, const Particles &input);

#endif
