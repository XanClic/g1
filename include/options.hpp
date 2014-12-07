#ifndef OPTIONS_HPP
#define OPTIONS_HPP

struct Options {
    int min_lod = 0, max_lod = 8;
    bool aurora = true;

    int scratch_map_resolution = 1080;
    bool uniform_scratch_map = false;

    int star_map_resolution = 2048;
};


extern Options global_options;

#endif
