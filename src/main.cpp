#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <string>

#include <dake/cross.hpp>

#include "graphics.hpp"
#include "main_loop.hpp"
#include "menu.hpp"
#include "options.hpp"
#include "ship_types.hpp"
#include "software.hpp"
#include "ui.hpp"
#include "weapons.hpp"

#include "json.hpp"


Options global_options;


int main(int argc, char *argv[])
{
    for (;;) {
        static const struct option options[] = {
            {"help", no_argument, nullptr, 'h'},
            {"min-lod", required_argument, nullptr, 256},
            {"max-lod", required_argument, nullptr, 257},
            {"disable-aurora", no_argument, nullptr, 258},
            {"scratch-map-res", required_argument, nullptr, 259},
            {"uniform-scratch-map", no_argument, nullptr, 260},
            {"star-map-res", required_argument, nullptr, 261},
            {"bloom", required_argument, nullptr, 262},

            {nullptr, 0, nullptr, 0}
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
                fprintf(stderr, "  --disable-aurora Disables aurora borealis and australis\n");
                fprintf(stderr, "  --scratch-map-res=resolution\n");
                fprintf(stderr, "                   Sets the vertical resolution of the scratch map\n");
                fprintf(stderr, "                   (720 or 1080)\n");
                fprintf(stderr, "  --uniform-scratch-map\n");
                fprintf(stderr, "                   Disables scratches in the scratch map\n");
                fprintf(stderr, "  --star-map-res=resolution\n");
                fprintf(stderr, "                   Sets the resolution of one side of the star map\n");
                fprintf(stderr, "                   (512, 1024 or 2048)\n");
                fprintf(stderr, "  --bloom=<no,lq,hq>\n");
                fprintf(stderr, "                   Chooses how to draw bloom (no: not at all; lq: with rather\n");
                fprintf(stderr, "                   low quality; hq (default): best quality available)\n");
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

            case 258:
                global_options.aurora = false;
                break;

            case 259: {
                char *endp;
                errno = 0;
                unsigned long res = strtoul(optarg, &endp, 0);
                if (errno || ((res != 720) && (res != 1080)) || *endp) {
                    fprintf(stderr, "Invalid argument given for --scratch-map-res (allowed: 720, 1080)\n");
                    return 1;
                }

                global_options.scratch_map_resolution = res;
                break;
            }

            case 260:
                global_options.uniform_scratch_map = true;
                break;

            case 261: {
                char *endp;
                errno = 0;
                unsigned long res = strtoul(optarg, &endp, 0);
                if (errno || ((res != 512) && (res != 1024) && (res != 2048)) || *endp) {
                    fprintf(stderr, "Invalid argument given for --star-map-res (allowed: 512, 1024, 2048)\n");
                    return 1;
                }

                global_options.star_map_resolution = res;
                break;
            }

            case 262:
                if (!strcmp(optarg, "no")) {
                    global_options.bloom_type = Options::NO_BLOOM;
                } else if (!strcmp(optarg, "lq")) {
                    global_options.bloom_type = Options::LQ_BLOOM;
                } else if (!strcmp(optarg, "hq")) {
                    global_options.bloom_type = Options::HQ_BLOOM;
                } else {
                    fprintf(stderr, "Invalid argument given for --bloom (allowed: no, lq, hq)\n");
                    return 1;
                }
                break;
        }
    }

    if (global_options.min_lod > global_options.max_lod) {
        fprintf(stderr, "min-lod may not exceed max-lod\n");
        return 1;
    }


    // timezones sure are awesome
    setenv("TZ", "", 1);
    tzset();


    init_ui();

    std::string scenario = menu_loop();

    init_game_graphics();
    load_software();
    load_ship_types();
    load_weapons();

    main_loop(scenario);


    return 0;
}
