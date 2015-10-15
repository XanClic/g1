#ifndef PARTICLES_HPP
#define PARTICLES_HPP

#include <dake/math/matrix.hpp>

#include <vector>


struct Particle {
    dake::math::vec<3, double> position;
    dake::math::vec3 position_relative_to_viewer;
    dake::math::vec3 velocity, orientation;
    float lifetime;
};


typedef std::vector<Particle> Particles;


struct WorldState;
struct GraphicsStatus;
struct ShipState;


void init_particles(void);

void spawn_particle(WorldState &output,
                    const dake::math::vec<3, double> &position,
                    const dake::math::vec3 &velocity,
                    const dake::math::vec3 &orientation);

void handle_particles(Particles &output, const Particles &input,
                      WorldState &out_ws, const ShipState &player);

void draw_particles(const GraphicsStatus &state, const Particles &input);

#endif
