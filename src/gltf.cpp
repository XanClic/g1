#include <dake/gl.hpp>

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <libgen.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "gltf.hpp"
#include "serializer.hpp"


using namespace dake;


class Buffer {
    private:
        void *ptr = nullptr;

    public:
        Buffer(void) {}
        Buffer(size_t sz) { ptr = malloc(sz); }
        ~Buffer(void) { free(ptr); }

        operator void *(void) { return ptr; }
        void *operator+(size_t off)
        { return static_cast<uint8_t *>(ptr) + off; }
};


static int element_count(GLTFAccessorType t)
{
    switch (t) {
        case SCALAR:
            return 1;
        case VEC2:
            return 2;
        case VEC3:
            return 3;

        default:
            abort();
    }
}


static std::string str_tolower(const std::string &str)
{
    size_t in_sz = str.size();

    std::string out(in_sz, 0);
    for (size_t i = 0; i < in_sz; i++) {
        out[i] = tolower(str[i]);
    }

    return out;
}


GLTFObject *GLTFObject::load(const std::string &name)
{
    char *dn = strdup(name.c_str());
    std::string gltf_dir(dirname(dn));
    free(dn);

    gltf_dir += "/";

    GLTF gltf;
    parse_file(&gltf, name);

    std::unordered_map<std::string, Buffer> buffers;

    for (const auto &b: gltf.buffers) {
        const std::string &fn = gl::find_resource_filename(gltf_dir +
                                                           b.second.uri);
        FILE *fp = fopen(fn.c_str(), "rb");
        if (!fp) {
            throw std::runtime_error("Failed to open resource " + b.second.uri +
                                     strerror(errno));
        }

        buffers.emplace(b.first, b.second.byteLength);
        if (fread(buffers[b.first], 1, b.second.byteLength, fp)
            < b.second.byteLength)
        {
            fclose(fp);
            throw std::runtime_error("Failed to read resource " + b.second.uri);
        }

        fclose(fp);
    }


    GLTFObject *obj = new GLTFObject;

    std::unordered_map<std::string, GLuint> buffer_views;

    for (const auto &bv: gltf.bufferViews) {
        glGenBuffers(1, &buffer_views[bv.first]);
        glBindBuffer(bv.second.target, buffer_views[bv.first]);
        glBufferData(bv.second.target, bv.second.byteLength,
                     buffers[bv.second.buffer] + bv.second.byteOffset,
                     GL_STATIC_DRAW);

        obj->gl_buffers.push_back(buffer_views[bv.first]);
    }


    GLuint attrib_counter = 0;
    for (const auto &m: gltf.meshes) {
        for (const auto &mp: m.second.primitives) {
            obj->primitives.emplace_back();
            MeshPrimitive &prim = obj->primitives.back();

            prim.mode = mp.mode;

            if (mp.has_indices) {
                const GLTFAccessor &acc = gltf.accessors[mp.indices];

                gl::elements_array *ea = prim.va.indices();
                ea->reuse_buffer(buffer_views[acc.bufferView]);
                ea->format(element_count(acc.type), acc.componentType);

                if (acc.byteOffset || acc.byteStride) {
                    delete obj;
                    throw std::runtime_error("Cannot specify offset or stride "
                                             "index accessors");
                }

                prim.vertices = acc.count;
            } else {
                const std::string &acc_name = mp.attributes.begin()->second;
                prim.vertices = gltf.accessors[acc_name].count;
            }

            prim.va.set_elements(prim.vertices);

            for (const auto &a: mp.attributes) {
                GLuint attr_index;

                const std::string &attr_name = str_tolower(a.first);

                auto ass = obj->attrib_assignment.find(attr_name);
                if (ass == obj->attrib_assignment.end()) {
                    attr_index = attrib_counter++;
                    obj->attrib_assignment[attr_name] = attr_index;
                } else {
                    attr_index = ass->second;
                }

                const GLTFAccessor &acc = gltf.accessors[a.second];

                gl::vertex_attrib *attr = prim.va.attrib(attr_index);
                attr->reuse_buffer(buffer_views[acc.bufferView]);
                attr->format(element_count(acc.type), acc.componentType);
                attr->load(acc.byteStride, acc.byteOffset);
            }
        }
    }


    return obj;
}


GLTFObject::~GLTFObject(void)
{
    primitives.clear();
    glDeleteBuffers(gl_buffers.size(), gl_buffers.data());
}


GLuint GLTFObject::attrib_index(const std::string &name)
{
    auto it = attrib_assignment.find(name);
    if (it == attrib_assignment.end()) {
        throw std::runtime_error("Failed to find vertex attribute " + name);
    }
    return it->second;
}


void GLTFObject::bind_program_vertex_attribs(gl::program &prg)
{
    for (const auto &aa: attrib_assignment) {
        prg.bind_attrib(("va_" + aa.first).c_str(), aa.second);
    }
}


gl::vertex_array &GLTFObject::vertex_array(int index)
{
    return primitives[index].va;
}


size_t GLTFObject::vertex_count(int index)
{
    return primitives[index].vertices;
}


void GLTFObject::draw(void)
{
    for (MeshPrimitive &mp: primitives) {
        mp.va.draw(mp.mode);
    }
}
