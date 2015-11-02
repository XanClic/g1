#ifndef GLTF_HPP
#define GLTF_HPP

#include <dake/gl.hpp>

#include <string>
#include <unordered_map>
#include <vector>


class GLTFObject {
    private:
        struct MeshPrimitive {
            MeshPrimitive(void);
            MeshPrimitive(const MeshPrimitive &mp) = delete;
            ~MeshPrimitive(void);

            MeshPrimitive(MeshPrimitive &&mp);

            dake::gl::vertex_array *va = nullptr;
            GLenum mode;
            size_t vertices;
        };

        std::vector<MeshPrimitive> primitives;
        std::vector<GLuint> gl_buffers;
        std::unordered_map<std::string, GLuint> attrib_assignment;


    public:
        ~GLTFObject(void);

        GLuint attrib_index(const std::string &name);
        void bind_program_vertex_attribs(dake::gl::program &prg);

        dake::gl::vertex_array &vertex_array(int index);
        size_t vertex_count(int va_index);

        void draw(void);

        static GLTFObject *load(const std::string &name);
};

#endif
