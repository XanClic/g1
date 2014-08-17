#ifndef XSMD_HPP
#define XSMD_HPP

#include <dake/dake.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>


struct XSMD {
    enum XSMDVertexMode {
        TRIANGLE_STRIP
    };

    enum XSMDDataType {
        FLOAT
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


    XSMDHeader *hdr = nullptr;
    XSMDVertexAttrib **attribs = nullptr;
    void *attrib_data = nullptr;
    size_t *strides = nullptr;

    std::vector<GLuint> *mapping = nullptr;
    bool may_not_remap = false;

    dake::gl::vertex_array *va = nullptr;

    ~XSMD(void);

    dake::gl::vertex_array *make_vertex_array(void);
    void draw(void);

    void from_program_mapping(dake::gl::program &prg);
    void bind_program_vertex_attribs(dake::gl::program &prg);
};


XSMD *load_xsmd(const std::string &name);

#endif
