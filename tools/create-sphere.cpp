#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include <dake/cross.hpp>
#include <dake/gl.hpp>
#include <dake/math.hpp>


using namespace dake;
using namespace dake::math;


int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: create-sphere <file.gltf> <horizontal segments> <vertical segments>\n");
        return 1;
    }

    if (strlen(argv[1]) < 5 || strcmp(argv[1] + strlen(argv[1]) - 5, ".gltf")) {
        fprintf(stderr, "Filename does not end in .gltf: %s\n", argv[1]);
        return 1;
    }

    char *endptr;
    errno = 0;

    long hor_count = strtol(argv[2], &endptr, 0);
    if ((hor_count < 3) || *endptr || errno) {
        fprintf(stderr, "Invalid horizontal segment count given\n");
        return 1;
    }

    long ver_count = strtol(argv[3], &endptr, 0);
    if ((ver_count < 2) || *endptr || errno) {
        fprintf(stderr, "Invalid vertical segment count given\n");
        return 1;
    }


    char *binfile = strdup(argv[1]);
    binfile[strlen(binfile) - 5] = 0;
    strcat(binfile, ".bin");

    FILE *gltffp = fopen(argv[1], "wb");
    FILE *binfp = fopen(binfile, "wb");

    struct Vertex {
        vec3 position;
        vec3 normal;
        vec2 texcoord;
    };

    int vertex_count = hor_count * (ver_count * 2 + 2) - 2;

    fprintf(gltffp, "{\n");
    fprintf(gltffp, "    \"buffers\": {\n");
    fprintf(gltffp, "        \"mesh_buffer\": {\n");
    fprintf(gltffp, "            \"byteLength\": %zi,\n", vertex_count * sizeof(Vertex));
    fprintf(gltffp, "            \"type\": \"arraybuffer\",\n");
    fprintf(gltffp, "            \"uri\": \"%s\"\n", basename(binfile));
    fprintf(gltffp, "        }\n");
    fprintf(gltffp, "    },\n");
    fprintf(gltffp, "    \"bufferViews\": {\n");
    fprintf(gltffp, "        \"mesh_bufferview\": {\n");
    fprintf(gltffp, "            \"buffer\": \"mesh_buffer\",\n");
    fprintf(gltffp, "            \"byteLength\": %zi,\n", vertex_count * sizeof(Vertex));
    fprintf(gltffp, "            \"byteOffset\": %i,\n", 0);
    fprintf(gltffp, "            \"target\": %i\n", GL_ARRAY_BUFFER);
    fprintf(gltffp, "        }\n");
    fprintf(gltffp, "    },\n");
    fprintf(gltffp, "    \"accessors\": {\n");
    fprintf(gltffp, "        \"acc_position\": {\n");
    fprintf(gltffp, "            \"bufferView\": \"mesh_bufferview\",\n");
    fprintf(gltffp, "            \"byteOffset\": %zi,\n", offsetof(Vertex, position));
    fprintf(gltffp, "            \"byteStride\": %zi,\n", sizeof(Vertex));
    fprintf(gltffp, "            \"componentType\": %i,\n", GL_FLOAT);
    fprintf(gltffp, "            \"count\": %i,\n", vertex_count);
    fprintf(gltffp, "            \"type\": \"VEC3\"\n");
    fprintf(gltffp, "        },\n");
    fprintf(gltffp, "        \"acc_normal\": {\n");
    fprintf(gltffp, "            \"bufferView\": \"mesh_bufferview\",\n");
    fprintf(gltffp, "            \"byteOffset\": %zi,\n", offsetof(Vertex, normal));
    fprintf(gltffp, "            \"byteStride\": %zi,\n", sizeof(Vertex));
    fprintf(gltffp, "            \"componentType\": %i,\n", GL_FLOAT);
    fprintf(gltffp, "            \"count\": %i,\n", vertex_count);
    fprintf(gltffp, "            \"type\": \"VEC3\"\n");
    fprintf(gltffp, "        },\n");
    fprintf(gltffp, "        \"acc_texcoord\": {\n");
    fprintf(gltffp, "            \"bufferView\": \"mesh_bufferview\",\n");
    fprintf(gltffp, "            \"byteOffset\": %zi,\n", offsetof(Vertex, texcoord));
    fprintf(gltffp, "            \"byteStride\": %zi,\n", sizeof(Vertex));
    fprintf(gltffp, "            \"componentType\": %i,\n", GL_FLOAT);
    fprintf(gltffp, "            \"count\": %i,\n", vertex_count);
    fprintf(gltffp, "            \"type\": \"VEC2\"\n");
    fprintf(gltffp, "        }\n");
    fprintf(gltffp, "    },\n");
    fprintf(gltffp, "    \"meshes\": {\n");
    fprintf(gltffp, "        \"sphere\": {\n");
    fprintf(gltffp, "            \"primitives\": [\n");
    fprintf(gltffp, "                {\n");
    fprintf(gltffp, "                    \"attributes\": {\n");
    fprintf(gltffp, "                        \"POSITION\": \"acc_position\",\n");
    fprintf(gltffp, "                        \"NORMAL\": \"acc_normal\",\n");
    fprintf(gltffp, "                        \"TEXCOORD\": \"acc_texcoord\"\n");
    fprintf(gltffp, "                    },\n");
    fprintf(gltffp, "                    \"mode\": %i\n", GL_TRIANGLE_STRIP);
    fprintf(gltffp, "                }\n");
    fprintf(gltffp, "            ]\n");
    fprintf(gltffp, "        }\n");
    fprintf(gltffp, "    }\n");
    fprintf(gltffp, "}\n");

    fclose(gltffp);

    Vertex *vertices = new Vertex[vertex_count];

    long vi = 0;

    for (long hor = 0; hor < hor_count; hor++) {
        float hangle[2] = {
            2.f * static_cast<float>(M_PI) *  hor      / hor_count,
            2.f * static_cast<float>(M_PI) * (hor + 1) / hor_count
        };

        for (int i = 0; i < (hor ? 2 : 1); i++) {
            vertices[vi].position = vec3(0.f, 1.f, 0.f);
            vertices[vi].normal   = vertices[vi].position.normalized();
            vertices[vi].texcoord = vec2(1.f - (hangle[0] + hangle[1]) / (4.f * static_cast<float>(M_PI)), 0.f);
            vi++;
        }

        for (long ver = 0; ver < ver_count - 1; ver++) {
            float vangle = (ver + 1.f) / ver_count * static_cast<float>(M_PI);

            for (int i: {1, 0}) {
                vertices[vi].position = vec3(sinf(vangle) * cosf(hangle[i]), cosf(vangle), sinf(vangle) * sinf(hangle[i]));
                vertices[vi].normal   = vertices[vi].position.normalized();
                vertices[vi].texcoord = vec2(1.f - hangle[i] / (2.f * static_cast<float>(M_PI)), vangle / static_cast<float>(M_PI));
                vi++;
            }
        }

        for (int i = 0; i < (hor < hor_count - 1 ? 2 : 1); i++) {
            vertices[vi].position = vec3(0.f, -1.f, 0.f);
            vertices[vi].normal   = vertices[vi].position.normalized();
            vertices[vi].texcoord = vec2(1.f - (hangle[0] + hangle[1]) / (4.f * static_cast<float>(M_PI)), 1.f);
            vi++;
        }
    }

    assert(vi == static_cast<long>(vertex_count));


    fwrite(vertices, sizeof(Vertex), vertex_count, binfp);

    fclose(binfp);


    return 0;
}
