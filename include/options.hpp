#ifndef OPTIONS_HPP
#define OPTIONS_HPP

struct Options {
    int min_lod = 0, max_lod = 8;
    bool aurora = true;
};


extern Options global_options;

#endif
