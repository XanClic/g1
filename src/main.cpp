#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>

#include "main_loop.hpp"
#include "options.hpp"
#include "ui.hpp"


Options global_options;


int main(int argc, char *argv[])
{
    for (;;) {
        static const struct option options[] = {
            {"help", no_argument, nullptr, 'h'},
            {"min-lod", required_argument, nullptr, 256},
            {"max-lod", required_argument, nullptr, 257},
        };

        int option = getopt_long(argc, argv, "h", options, nullptr);
        if (option == -1) {
            break;
        }

        switch (option) {
            case 'h':
            case '?':
                fprintf(stderr, "Usage: %s [options...]\n\n", argv[0]);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "  -h, --help       Shows this information\n");
                fprintf(stderr, "  --min-lod=LOD    Sets the minimum LOD (0..8; default: 0)\n");
                fprintf(stderr, "  --max-lod=LOD    Sets the maximum LOD (3..8; default: 8)\n");
                return 0;

            case 256: {
                char *endp;
                errno = 0;
                unsigned long min_lod = strtoul(optarg, &endp, 0);
                if (errno || (min_lod > 8) || *endp) {
                    fprintf(stderr, "Invalid argument given for --min-lod\n");
                    return 1;
                }

                global_options.min_lod = min_lod;
                break;
            }

            case 257: {
                char *endp;
                errno = 0;
                unsigned long max_lod = strtoul(optarg, &endp, 0);
                if (errno || (max_lod < 3) || (max_lod > 8) || *endp) {
                    fprintf(stderr, "Invalid argument given for --max-lod\n");
                    return 1;
                }

                global_options.max_lod = max_lod;
                break;
            }
        }
    }

    if (global_options.min_lod > global_options.max_lod) {
        fprintf(stderr, "min-lod may not exceed max-lod\n");
        return 1;
    }

    init_ui();

    main_loop();

    return 0;
}
