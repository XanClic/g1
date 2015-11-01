#ifndef SOUND_HPP
#define SOUND_HPP

#include <string>

#include <SDL2/SDL_mixer.h>

#include "physics.hpp"


class SoundEffect {
    private:
        Mix_Chunk *sdl_chunk = nullptr;


    public:
        SoundEffect(const std::string &name);
        ~SoundEffect(void);

        void play(void);
};


void init_sound(void);
void do_sound(const WorldState &input);

#endif
