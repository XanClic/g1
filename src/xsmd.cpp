#include <dake/dake.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#include "xsmd.hpp"


static const GLenum XSMDGLVertexModes[] = {
    GL_TRIANGLE_STRIP
};

static const GLenum XSMDGLDataTypes[] = {
    GL_FLOAT
};

static const size_t XSMDDataTypeSize[] = {
    sizeof(float)
};


using namespace dake;


XSMD::~XSMD(void)
{
    delete va;
    delete mapping;

    free(attrib_data);
    delete[] strides;

    if (attribs) {
        for (unsigned i = 0; i < hdr->vertex_attrib_count; i++) {
            delete attribs[i];
        }

        delete[] attribs;
    }

    delete hdr;
}


void XSMD::from_program_mapping(gl::program &prg)
{
    if (mapping || may_not_remap) {
        throw std::runtime_error("Cannot remap XSMD VA mapping");
    }

    mapping = new std::vector<GLuint>;

    for (unsigned i = 0; i < hdr->vertex_attrib_count; i++) {
        mapping->push_back(prg.attrib(attribs[i]->name));
    }

    may_not_remap = true;
}


void XSMD::bind_program_vertex_attribs(gl::program &prg)
{
    for (unsigned i = 0; i < hdr->vertex_attrib_count; i++) {
        prg.bind_attrib(attribs[i]->name, mapping ? (*mapping)[i] : i);
    }

    may_not_remap = true;
}


gl::vertex_array *XSMD::make_vertex_array(void)
{
    if (va) {
        return va;
    }

    va = new gl::vertex_array;
    va->set_elements(hdr->vertex_count);

    gl::vertex_attrib *attr0 = nullptr;

    for (unsigned i = 0; i < hdr->vertex_attrib_count; i++) {
        gl::vertex_attrib *attr = va->attrib(mapping ? (*mapping)[i] : i);
        attr->format(attribs[i]->elements_per_vertex, XSMDGLDataTypes[attribs[i]->element_type]);

        if (attr0) {
            attr->reuse_buffer(attr0);
        } else {
            attr->data(attrib_data, hdr->vertex_count * strides[hdr->vertex_attrib_count]);
        }

        attr->load(strides[hdr->vertex_attrib_count], strides[i]);
    }

    may_not_remap = true;

    return va;
}


void XSMD::draw(void)
{
    va->draw(XSMDGLVertexModes[hdr->vertex_mode]);
}


XSMD *load_xsmd(const std::string &name)
{
    std::string full_name = gl::find_resource_filename(name);

    FILE *fp = fopen(full_name.c_str(), "rb");
    if (!fp) {
        throw std::invalid_argument("Could not open XSMD " + name + ": " + std::string(strerror(errno)));
    }

    XSMD::XSMDHeader *hdr = new XSMD::XSMDHeader;
    fread(hdr, sizeof(*hdr), 1, fp);

    if (strncmp(hdr->magic, "XSOGLVDD", sizeof(hdr->magic))) {
        throw std::invalid_argument("Given file " + name + " is not an XSMD file");
    }

    if (strncmp(hdr->version, "NPRJAW", sizeof(hdr->version))) {
        throw std::invalid_argument("Given file " + name + " has an unknown XSMD version");
    }

    if (hdr->incompatible_features) {
        throw std::invalid_argument("Given XSMD file " + name + " has incompatible features");
    }


    fseek(fp, hdr->vertex_attrib_headers_offset, SEEK_SET);

    XSMD::XSMDVertexAttrib **ahdrs = static_cast<XSMD::XSMDVertexAttrib **>(malloc(hdr->vertex_attrib_count * sizeof(*ahdrs)));
    size_t *strides = new size_t[hdr->vertex_attrib_count + 1];
    strides[0] = 0;

    for (unsigned i = 0; i < hdr->vertex_attrib_count; i++) {
        ahdrs[i] = static_cast<XSMD::XSMDVertexAttrib *>(malloc(sizeof(*ahdrs[i])));

        fread(ahdrs[i], sizeof(*ahdrs[i]), 1, fp);

        ahdrs[i] = static_cast<XSMD::XSMDVertexAttrib *>(realloc(ahdrs[i], sizeof(*ahdrs[i]) + ahdrs[i]->name_length + 1));
        fread(ahdrs[i]->name, 1, ahdrs[i]->name_length, fp);
        ahdrs[i]->name[ahdrs[i]->name_length] = 0;

        strides[i + 1] = strides[i] + ahdrs[i]->elements_per_vertex * XSMDDataTypeSize[ahdrs[i]->element_type];
    }

    fseek(fp, hdr->vertex_data_offset, SEEK_SET);

    void *attr_data = malloc(hdr->vertex_count * strides[hdr->vertex_attrib_count]);
    fread(attr_data, strides[hdr->vertex_attrib_count], hdr->vertex_count, fp);

    fclose(fp);


    XSMD *xsmd = new XSMD;

    xsmd->hdr         = new (hdr) XSMD::XSMDHeader;
    xsmd->attribs     = new XSMD::XSMDVertexAttrib *[hdr->vertex_attrib_count];
    xsmd->attrib_data = attr_data;
    xsmd->strides     = strides;

    for (unsigned i = 0; i < hdr->vertex_attrib_count; i++) {
        xsmd->attribs[i] = new (ahdrs[i]) XSMD::XSMDVertexAttrib;
    }

    free(ahdrs);


    return xsmd;
}
