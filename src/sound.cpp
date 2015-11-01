#include <stdexcept>
#include <string>
#include <unordered_map>

#include <dake/gl/find_resource.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include "physics.hpp"
#include "ship.hpp"
#include "sound.hpp"
#include "steam_controller.hpp"
#include "weapons.hpp"


static std::unordered_map<std::string, SoundEffect> sfx;


void init_sound(void)
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        throw std::runtime_error("Failed to initialize audio subsystem");
    }

    Mix_AllocateChannels(32);
}


static SoundEffect &get_sfx(const std::string &name)
{
    auto it = sfx.find(name);
    if (it == sfx.end()) {
        return sfx.emplace(name, name).first->second;
    } else {
        return it->second;
    }
}


void do_sound(const WorldState &input)
{
    (void)input;
}


SoundEffect::SoundEffect(const std::string &file)
{
    std::string fname = dake::gl::find_resource_filename("assets/sounds/" +
                                                         file);

    sdl_chunk = Mix_LoadWAV(fname.c_str());
    if (!sdl_chunk) {
        throw std::runtime_error("Failed to load sound file " + fname);
    }
}


SoundEffect::~SoundEffect(void)
{
    Mix_FreeChunk(sdl_chunk);
}


void SoundEffect::play(void)
{
    Mix_PlayChannel(-1, sdl_chunk, 0);
}
