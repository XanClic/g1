#include <cassert>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <dake/math.hpp>
#include <dake/gl/texture.hpp>


using namespace dake::gl;
using namespace dake::math;


int main(int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: polar-to-cube <side> <out_resolution> <input> <output.ppm>\n");
        fprintf(stderr, "side \\in { right, left, top, bottom, front, back }\n");
        return 1;
    }

    float x = 0.f, y = 0.f, z = 0.f;
    float *h, *v;

    if (!strcmp(argv[1], "right")) {
        x = 1.f;
        h = &z;
        v = &y;
    } else if (!strcmp(argv[1], "left")) {
        x = -1.f;
        h = &z;
        v = &y;
    } else if (!strcmp(argv[1], "top")) {
        y = 1.f;
        h = &x;
        v = &z;
    } else if (!strcmp(argv[1], "bottom")) {
        y = -1.f;
        h = &x;
        v = &z;
    } else if (!strcmp(argv[1], "front")) {
        z = 1.f;
        h = &x;
        v = &y;
    } else if (!strcmp(argv[1], "back")) {
        z = -1.f;
        h = &x;
        v = &y;
    } else {
        fprintf(stderr, "Invalid side given\n");
        return 1;
    }

    char *endptr;
    errno = 0;

    long outres = strtol(argv[2], &endptr, 0);
    if ((outres <= 0) || (outres >= LONG_MAX / outres) || *endptr || errno) {
        fprintf(stderr, "Invalid output resolution given\n");
        return 1;
    }

    image in(argv[3]);

    vec<3, uint8_t> *out = new vec<3, uint8_t>[outres * outres];

    for (long vi = 0; vi < outres; vi++) {
        *v = (static_cast<float>(vi) / (outres - 1) - .5f) * 2.f;

        for (long hi = 0; hi < outres; hi++) {
            *h = (static_cast<float>(hi) / (outres - 1) - .5f) * 2.f;

            float radius = vec3(x, y, z).length();
            float vangle = asinf(y / radius) + M_PIf / 2.f;
            float hangle = M_PIf - atan2f(z, x);

            float in_x = hangle / (2.f * M_PIf) * in.width();
            float in_y = vangle / M_PIf         * in.height();

            float in_xi, in_yi;

            float in_xr = modff(in_x, &in_xi);
            float in_yr = modff(in_y, &in_yi);

            int in_xint = lrintf(in_xi);
            int in_yint = lrintf(in_yi);

            assert(in.channels() == 3 || in.channels() == 4);
            assert(in.format() == image::LINEAR_UINT8);

            uint8_t in_r00 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint    )              + ((in_yint    )              ) * in.width())    ];
            uint8_t in_g00 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint    )              + ((in_yint    )              ) * in.width()) + 1];
            uint8_t in_b00 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint    )              + ((in_yint    )              ) * in.width()) + 2];

            uint8_t in_r10 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint + 1) % in.width() + ((in_yint    )              ) * in.width())    ];
            uint8_t in_g10 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint + 1) % in.width() + ((in_yint    )              ) * in.width()) + 1];
            uint8_t in_b10 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint + 1) % in.width() + ((in_yint    )              ) * in.width()) + 2];

            uint8_t in_r01 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint    )              + ((in_yint + 1) % in.height()) * in.width())    ];
            uint8_t in_g01 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint    )              + ((in_yint + 1) % in.height()) * in.width()) + 1];
            uint8_t in_b01 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint    )              + ((in_yint + 1) % in.height()) * in.width()) + 2];

            uint8_t in_r11 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint + 1) % in.width() + ((in_yint + 1) % in.height()) * in.width())    ];
            uint8_t in_g11 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint + 1) % in.width() + ((in_yint + 1) % in.height()) * in.width()) + 1];
            uint8_t in_b11 = static_cast<const uint8_t *>(in.data())[in.channels() * ((in_xint + 1) % in.width() + ((in_yint + 1) % in.height()) * in.width()) + 2];

            float in_r = in_r00 * (1.f - in_xr) * (1.f - in_yr)
                       + in_r10 *        in_xr  * (1.f - in_yr)
                       + in_r01 * (1.f - in_xr) *        in_yr
                       + in_r11 *        in_xr  *        in_yr;

            float in_g = in_g00 * (1.f - in_xr) * (1.f - in_yr)
                       + in_g10 *        in_xr  * (1.f - in_yr)
                       + in_g01 * (1.f - in_xr) *        in_yr
                       + in_g11 *        in_xr  *        in_yr;

            float in_b = in_b00 * (1.f - in_xr) * (1.f - in_yr)
                       + in_b10 *        in_xr  * (1.f - in_yr)
                       + in_b01 * (1.f - in_xr) *        in_yr
                       + in_b11 *        in_xr  *        in_yr;

            out[hi + vi * outres].r() = lrintf(in_r);
            out[hi + vi * outres].g() = lrintf(in_g);
            out[hi + vi * outres].b() = lrintf(in_b);
        }
    }

    FILE *fp = fopen(argv[4], "w");
    if (!fp) {
        perror("Failed to open output file");
        return 1;
    }

    fprintf(fp, "P6 %li %li 255\n", outres, outres);
    fwrite(out, 3, outres * outres, fp);

    fclose(fp);
    delete[] out;

    return 0;
}
