#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include <dake/math.hpp>


using namespace dake::math;


namespace XSMD
{

enum XSMDVertexMode {
    TRIANGLE_STRIP
};

enum XSMDDataType {
    FLOAT
};

static const size_t XSMDDataTypeSize[] = {
    sizeof(float)
};

struct XSMDHeader {
    char magic[8];
    char version[6];

    uint8_t compatible_features;
    uint8_t incompatible_features;

    uint64_t vertex_count;
    uint32_t vertex_mode;

    uint32_t vertex_attrib_count;

    uint64_t vertex_attrib_headers_offset;
    uint64_t vertex_data_offset;
} __attribute__((packed));

struct XSMDVertexAttrib {
    uint32_t element_type;
    uint32_t elements_per_vertex;
    uint32_t name_length;
    char name[];
} __attribute__((packed));

};


int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: create-sphere <file.xsmd> <horizontal segments> <vertical segments>\n");
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

    FILE *fp = fopen(argv[1], "wb");

    XSMD::XSMDHeader *hdr = new XSMD::XSMDHeader;

    memcpy(&hdr->magic, "XSOGLVDD", sizeof(hdr->magic)); // XanClic's Shitty OpenGL Vertex Data Dump (or XSMD, XanClic's Shitty Mesh Dump, for short)
    memcpy(&hdr->version, "NPRJAW", sizeof(hdr->version)); // No Parsing Required, Just Add Water

    hdr->compatible_features   = 0;
    hdr->incompatible_features = 0;

    hdr->vertex_mode  = XSMD::TRIANGLE_STRIP;
    hdr->vertex_count = hor_count * (ver_count * 2 + 2) - 2;

    hdr->vertex_attrib_count = 3;


    struct Vertex {
        vec3 position;
        vec3 normal;
        vec2 texcoord;
    };

    Vertex *vertices = new Vertex[hdr->vertex_count];

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

    assert(vi == static_cast<long>(hdr->vertex_count));


    XSMD::XSMDVertexAttrib *ahdrs[3] = {
        static_cast<XSMD::XSMDVertexAttrib *>(malloc(sizeof(*ahdrs[0]) + strlen("va_position"))),
        static_cast<XSMD::XSMDVertexAttrib *>(malloc(sizeof(*ahdrs[1]) + strlen("va_normal"))),
            static_cast<XSMD::XSMDVertexAttrib *>(malloc(sizeof(*ahdrs[2]) + strlen("va_texcoord")))
    };

    ahdrs[0]->element_type        = XSMD::FLOAT;
    ahdrs[0]->elements_per_vertex = 3;
    ahdrs[0]->name_length         = strlen("va_position");
    memcpy(ahdrs[0]->name, "va_position", strlen("va_position"));

    ahdrs[1]->element_type        = XSMD::FLOAT;
    ahdrs[1]->elements_per_vertex = 3;
    ahdrs[1]->name_length         = strlen("va_normal");
    memcpy(ahdrs[1]->name, "va_normal", strlen("va_normal"));

    ahdrs[2]->element_type        = XSMD::FLOAT;
    ahdrs[2]->elements_per_vertex = 2;
    ahdrs[2]->name_length         = strlen("va_texcoord");
    memcpy(ahdrs[2]->name, "va_texcoord", strlen("va_texcoord"));

    hdr->vertex_attrib_headers_offset = sizeof(*hdr);
    hdr->vertex_data_offset           = hdr->vertex_attrib_headers_offset;
    for (int i: {0, 1, 2}) {
        hdr->vertex_data_offset += sizeof(*ahdrs[i]) + ahdrs[i]->name_length;
    }


    fwrite(hdr, sizeof(*hdr), 1, fp);
    assert(ftell(fp) == static_cast<long>(hdr->vertex_attrib_headers_offset));

    for (int i: {0, 1, 2}) {
        fwrite(ahdrs[i], sizeof(*ahdrs[i]) + ahdrs[i]->name_length, 1, fp);
    }
    assert(ftell(fp) == static_cast<long>(hdr->vertex_data_offset));

    fwrite(vertices, sizeof(Vertex), hdr->vertex_count, fp);

    fclose(fp);


    return 0;
}
