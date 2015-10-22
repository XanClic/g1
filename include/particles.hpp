#ifndef PARTICLES_HPP
#define PARTICLES_HPP

#include <dake/math/matrix.hpp>

#include <vector>


struct ParticleGraphicsData {
    dake::math::vec3 position_relative_to_viewer;
    dake::math::vec3 orientation;
};

struct ParticleNonGraphicsData {
    dake::math::vec<3, double> position;
    dake::math::vec3 velocity;
    float lifetime;
    uint64_t source_ship_id;
};


struct Particles {
    std::vector<ParticleGraphicsData> pgd;
    std::vector<ParticleNonGraphicsData> pngd;
};


struct WorldState;
struct GraphicsStatus;
struct ShipState;


void init_particles(void);

void spawn_particle(WorldState &output, const ShipState &sender,
                    const dake::math::vec<3, double> &position,
                    const dake::math::vec3 &velocity,
                    const dake::math::vec3 &orientation);

void handle_particles(Particles &output, const Particles &input,
                      WorldState &out_ws, const ShipState &player);

void draw_particles(const GraphicsStatus &state, const Particles &input);

#endif
